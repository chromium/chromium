#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import build.android.gyp.util.build_utils as build_utils  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE_PROGUARD = os.environ.get('REBASELINE_PROGUARD', '0') != '0'

def CompareGeneratedWithGolden(generated_file_path, golden_file_path):
  golden_text = cronet_utils.read_file(golden_file_path)
  generated_text = cronet_utils.read_file(generated_file_path)
  if _REBASELINE_PROGUARD:
    if golden_text != generated_text:
      print('Updated', golden_file_path)
      with open(golden_file_path, 'w') as f:
        f.write(generated_text)
    return None

  if golden_text is None:
    print(f'Golden file does not exist: {golden_file_path}')

  return cronet_utils.compare_text_and_generate_diff(generated_text,
                                                     golden_text,
                                                     golden_file_path)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--input_generated_file',
                      type=str,
                      help="Path to the generated file.")
  parser.add_argument('--input_golden_file',
                      type=str,
                      help='Path to the input golden file.')
  parser.add_argument('--stamp', type=str, help='Path to touch on success')
  args = parser.parse_args()
  text_diff = CompareGeneratedWithGolden(args.input_generated_file,
                                         args.input_golden_file)
  if text_diff:

    print(f"""
Cronet Proguard golden test failed. To generate it:
#######################################################
#                                                     #
#      Run the command below to generate the file     #
#                                                     #
#######################################################

########### START ###########
patch -p1 <<'END_DIFF'
{text_diff}
END_DIFF
############ END ############

If you wish to build the action locally in generation mode, See the instructions below:
#####################################################################
#                                                                   #
# Run the command below to re-run the action in generation mode     #
#                                                                   #
#####################################################################
./components/cronet/tools/cr_cronet.py -d out/proguard_config gn && \
REBASELINE_PROGUARD=1 autoninja -C out/proguard_config \
cronet_combined_proguard_flags_golden_test

This will generate a GN output directory with the appropriate GN\
flags to run the golden_test in generation mode rather than verification mode.
""")
    sys.exit(-1)
  else:
    build_utils.Touch(args.stamp)

if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import pathlib
import os
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position

# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE_CRONET_PROGUARD = os.environ.get('REBASELINE_CRONET_PROGUARD',
                                             '0') != '0'


def _ReadFile(path):
  """Reads a file as a string"""
  return pathlib.Path(path).read_text()


def _CompareText(generated_text, golden_text, golden_file_path):
  """
  Compares the generated text with the golden text.

  returns a diff that can be applied with `patch` if exists.
  """
  golden_lines = [line.rstrip() for line in golden_text.splitlines()]
  generated_lines = [line.rstrip() for line in generated_text.splitlines()]
  if golden_lines == generated_lines:
    return None

  expected_path = os.path.relpath(
      golden_file_path, build_utils.DIR_SOURCE_ROOT)

  diff = difflib.unified_diff(
      golden_lines,
      generated_lines,
      fromfile=os.path.join('before', expected_path),
      tofile=os.path.join('after', expected_path),
      n=0,
      lineterm='',
  )

  return '\n'.join(diff)


def CompareGeneratedWithGolden(generated_file_path, golden_file_path):
  golden_text = _ReadFile(golden_file_path)
  generated_text = _ReadFile(generated_file_path)
  if _REBASELINE_CRONET_PROGUARD:
    if golden_text != generated_text:
      print('Updated', golden_file_path)
      with open(golden_file_path, 'w') as f:
        f.write(generated_text)
    return None

  if golden_text is None:
    print(f'Golden file does not exist: {golden_file_path}')

  return _CompareText(generated_text, golden_text, golden_file_path)


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
patch -p1 << 'END_DIFF'
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
REBASELINE_CRONET_PROGUARD=1 autoninja -C out/proguard_config \
cronet_combined_proguard_flags_golden_test

This will generate a GN output directory with the appropriate GN\
flags to run the golden_test in generation mode rather than verification mode.
""")
    sys.exit(-1)
  else:
    build_utils.Touch(args.stamp)

if __name__ == '__main__':
  sys.exit(main())

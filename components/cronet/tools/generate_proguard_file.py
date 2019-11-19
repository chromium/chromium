#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool that combines a sequence of input proguard files and outputs a single
# proguard file.
#
# The final output file is formed by concatenating all of the
# input proguard files, and then sequentally applying any .patch files that
# were given in the input.
#
# This tool requires the ability to shell execute the 'patch' tool, and is
# expected to only be run on Linux.

import optparse
import sys
import subprocess


def ReadFile(path):
  with open(path, 'rb') as f:
    return f.read()


def IsPatchFile(path):
  return path.endswith('.patch')


def ApplyPatch(output_file, patch_file):
  try:
    subprocess.check_call(['patch', '--quiet', output_file, patch_file])
  except:
    message = '''
Failed applying patch %s to %s

For help on fixing read the documentation in the patch file.

'''
    sys.stderr.write(message % (patch_file, output_file))
    raise


def main():
  parser = optparse.OptionParser()
  parser.add_option('--output-file',
          help='Output file for the generated proguard file')

  options, input_files = parser.parse_args()

  proguard_files = [path for path in input_files if not IsPatchFile(path)]
  patch_files = [path for path in input_files if IsPatchFile(path)]

  # Concatenate all the proguard files.
  with open(options.output_file, 'wb') as target:
    for input_file in proguard_files:
      target.write(ReadFile(input_file))

  # Apply any patch files.
  for patch_file in patch_files:
    ApplyPatch(options.output_file, patch_file)


if __name__ == '__main__':
  sys.exit(main())

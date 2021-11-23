#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import shutil
import sys
import tempfile

def run(command, extra_options=''):
  command = command + ' ' + extra_options
  print command
  ret = os.system(command)
  if ret != 0:
    raise OSError(ret)


def GenerateIdlBindings(output_path, input_files):
  bytecode_path = tempfile.mkdtemp('idl_bytecode')
  generator = 'components/cronet/tools/generators/cronet_bindings_generator.py'
  input_file = 'components/cronet/native/cronet.idl'
  # Precompile bindings templates
  run(generator + ' precompile -o ', bytecode_path)
  # Generate C interface.
  run(generator + ' --use_bundled_pylibs generate ' + input_file +
      ' --bytecode_path ' + bytecode_path + ' -g c')
  # TODO(mef): Use output_path to put generated code into
  # out/<blah>/gen/components/cronet/native directory.
  # -o ' + output_path)
  # Format generated code.
  run('git cl format --full components/cronet/native')
  shutil.rmtree(bytecode_path)


def main():
  parser = optparse.OptionParser()
  parser.add_option('--output-path',
        help='Output path for generated bindings')

  options, input_files = parser.parse_args()
  GenerateIdlBindings(options.output_path, input_files)


if __name__ == '__main__':
  sys.exit(main())

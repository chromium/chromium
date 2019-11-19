#!/usr/bin/env python

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Create a static library which exposes only symbols which are explicitly marked
# as visible e.g., by __attribute__((visibility("default"))).
#
# See BUILD.gn in this directory for usage example.
#
# This way, we can reduce risk of symbol conflict when linking it into apps
# by exposing internal symbols, especially in third-party libraries.

import glob
import optparse
import os
import subprocess


# Mapping from GN's target_cpu attribute to ld's -arch parameter.
# Taken from the definition of config("compiler") in:
#   //build/config/mac/BUILD.gn
GN_CPU_TO_LD_ARCH = {
  'x64': 'x86_64',
  'x86': 'i386',
  'armv7': 'armv7',
  'arm': 'armv7',
  'arm64': 'arm64',
}


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '--input_libs',
      help='Comma-separated paths to input .a files which contain symbols '
           'which must be always linked.')
  parser.add_option(
      '--deps_lib',
      help='The path to a complete static library (.a file) which contains all '
           'dependencies of --input_libs. .o files in this library are also '
           'added to the output library, but only if they are referred from '
           '--input_libs.')
  parser.add_option(
      '--output_obj',
      help='Outputs the generated .o file here. This is an intermediate file.')
  parser.add_option(
      '--output_lib',
      help='Outputs the generated .a file here.')
  parser.add_option(
      '--current_cpu',
      help='The current processor architecture in the format of the target_cpu '
           'attribute in GN.')
  parser.add_option(
      '--use_custom_libcxx', default=False, action='store_true',
      help='Confirm there is a custom libc++ linked in.')
  (options, args) = parser.parse_args()
  assert not args

  developer_dir = subprocess.check_output(
      ['xcode-select', '--print-path']).strip()

  xctoolchain_libs = glob.glob(developer_dir
      + '/Toolchains/XcodeDefault.xctoolchain/usr/lib'
      + '/clang/*/lib/darwin/*.ios.a')
  print "Adding xctoolchain_libs: ", xctoolchain_libs

  # ld -r concatenates multiple .o files and .a files into a single .o file,
  # while "hiding" symbols not marked as visible.
  command = [
    'xcrun', 'ld',
    '-arch', GN_CPU_TO_LD_ARCH[options.current_cpu],
    '-r',
  ]
  for input_lib in options.input_libs.split(','):
    # By default, ld only pulls .o files out of a static library if needed to
    # resolve some symbol reference. We apply -force_load option to input_lib
    # (but not to deps_lib) to force pulling all .o files.
    command += ['-force_load', input_lib]
  command += xctoolchain_libs
  command += [
    options.deps_lib,
    '-o', options.output_obj
  ]
  try:
    subprocess.check_call(command)
  except subprocess.CalledProcessError:
    # Work around LD failure for x86 Debug buiilds when it fails with error:
    # ld: scattered reloc r_address too large for architecture i386
    if options.current_cpu == "x86":
      # Combmine input lib with dependencies into output lib.
      command = [
        'xcrun', 'libtool', '-static', '-no_warning_for_no_symbols',
        '-o', options.output_lib,
        options.input_libs, options.deps_lib,
      ]
      subprocess.check_call(command)
      # Strip debug info from output lib so its size doesn't exceed 512mb.
      command = [
        'xcrun', 'strip', '-S', options.output_lib,
      ]
      subprocess.check_call(command)
      return
    else:
      exit(1)

  if os.path.exists(options.output_lib):
    os.remove(options.output_lib)

  # Creates a .a file which contains a single .o file.
  command = [
    'xcrun', 'ar', '-r',
    options.output_lib,
    options.output_obj,
  ]
  subprocess.check_call(command)

  if options.use_custom_libcxx:
    ret = os.system('xcrun nm -u "' + options.output_obj + '" | grep ___cxa_pure_virtual')
    if ret == 0:
      print "ERROR: Found undefined libc++ symbols, is libc++ indcluded in dependencies?"
      exit(2)


if __name__ == "__main__":
  main()

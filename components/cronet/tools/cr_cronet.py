#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
cr_cronet.py - cr - like helper tool for cronet developers
"""

import argparse
import sys
import os
import shlex
from datetime import datetime

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
from components.cronet.tools.utils import run, run_shell, android_gn_gen, build  # pylint: disable=wrong-import-position


def install(out_dir):
  cmd = ['build/android/adb_install_apk.py']
  # Propagate PATH to avoid issues with missing tools http://crbug/1217979
  env = {
      'BUILDTYPE': out_dir[4:],
      'PATH': os.environ.get('PATH', ''),
      'HOME': os.environ.get('HOME', '')
  }
  return run(cmd + ['CronetTestInstrumentation.apk'], env=env)


def test(out_dir, extra_options):
  # Ideally we would fetch this path from somewhere. Though, that's not trivial
  # and very unlikely to change. This being "best effort test code", it should
  # be fine just to hardcode it.
  remote_netlog_dir = '/data/data/org.chromium.net.tests/app_cronet_test/NetLog'
  run(['adb', 'shell', 'rm', '-rf', remote_netlog_dir])
  run([out_dir + '/bin/run_cronet_test_instrumentation_apk'] + extra_options)
  local_netlog_dir = out_dir + '/netlogs_for-' + datetime.now().strftime(
      "%y_%m_%d-%H_%M_%S")
  return run(['adb', 'pull', remote_netlog_dir, local_netlog_dir])


def unittest(out_dir, extra_options):
  return run([out_dir + '/bin/run_cronet_unittests_android'] + extra_options)


def debug(extra_options):
  return run([
      'build/android/adb_gdb', '--start', '--activity=.CronetTestActivity',
      '--program-name=CronetTest', '--package-name=org.chromium.net'
  ] + extra_options)


def stack(out_dir):
  return run_shell('adb logcat -d | CHROMIUM_OUTPUT_DIR=' +
                   shlex.quote(out_dir) +
                   ' third_party/android_platform/development/scripts/stack')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('command',
                      choices=[
                          'gn', 'sync', 'build', 'install', 'proguard', 'test',
                          'build-test', 'unit', 'build-unit', 'stack', 'debug',
                          'build-debug'
                      ])
  parser.add_argument('-d',
                      '--out_dir',
                      action='store',
                      help='name of the build directory')
  parser.add_argument('-x',
                      '--x86',
                      action='store_true',
                      help='build for Intel x86 architecture')
  parser.add_argument('--x64',
                      action='store_true',
                      help='build for Intel x86_64 architecture')
  parser.add_argument('--arm',
                      action='store_true',
                      help='build for arm architecture')
  parser.add_argument('-R',
                      '--riscv64',
                      action='store_true',
                      help='build for riscv64 architecture')
  parser.add_argument('-r',
                      '--release',
                      action='store_true',
                      help='use release configuration')
  parser.add_argument('-a',
                      '--asan',
                      action='store_true',
                      help='use address sanitizer')

  options, extra_options = parser.parse_known_args()
  print("Options:", options)
  print("Extra options:", extra_options)

  test_target = 'cronet_test_instrumentation_apk'
  unit_target = 'cronet_unittests_android'
  if options.x86:
    target_cpu = 'x86'
    out_dir_suffix = '-x86'
  elif options.x64:
    target_cpu = 'x64'
    out_dir_suffix = '-x64'
  elif options.riscv64:
    target_cpu = 'riscv64'
    out_dir_suffix = '-riscv64'
  elif options.arm:
    target_cpu = 'arm'
    out_dir_suffix = '-arm'
  else:
    target_cpu = 'arm64'
    out_dir_suffix = '-arm64'

  if options.asan:
    # ASAN on Android requires one-time setup described here:
    # https://www.chromium.org/developers/testing/addresssanitizer
    out_dir_suffix += '-asan'

  if options.out_dir:
    out_dir = options.out_dir
  else:
    if options.release:
      out_dir = 'out/Release' + out_dir_suffix
    else:
      out_dir = 'out/Debug' + out_dir_suffix

  if (options.command == 'gn'):
    return android_gn_gen(options.release, target_cpu, out_dir)
  if (options.command == 'sync'):
    return run(['git', 'pull', '--rebase']) or run(['gclient', 'sync'])
  if (options.command == 'build'):
    return build(out_dir, test_target, extra_options)
  if (options.command == 'install'):
    return install(out_dir)
  if (options.command == 'proguard'):
    return build(out_dir, 'cronet_sample_proguard_apk')
  if (options.command == 'test'):
    return install(out_dir) or test(out_dir, extra_options)
  if (options.command == 'build-test'):
    return build(out_dir, test_target) or install(out_dir) or \
        test(out_dir, extra_options)
  if (options.command == 'stack'):
    return stack(out_dir)
  if (options.command == 'debug'):
    return install(out_dir) or debug(extra_options)
  if (options.command == 'build-debug'):
    return build(out_dir, test_target) or install(out_dir) or \
        debug(extra_options)
  if (options.command == 'unit'):
    return unittest(out_dir, extra_options)
  if (options.command == 'build-unit'):
    return build(out_dir, unit_target) or unittest(out_dir, extra_options)

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())

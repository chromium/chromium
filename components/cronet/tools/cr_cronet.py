#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
cr_cronet.py - cr - like helper tool for cronet developers
"""

import argparse
import os
import pipes
import subprocess
import sys


def quoted_args(args):
  return ' '.join([pipes.quote(arg) for arg in args])


def run(command, **kwargs):
  print(command, kwargs)
  return subprocess.call(command, **kwargs)


def run_shell(command, extra_options=''):
  command = command + ' ' + extra_options
  print(command)
  return os.system(command)


def gn(out_dir, gn_args, gn_extra=None):
  cmd = ['gn', 'gen', out_dir, "--args=%s" % gn_args]
  if gn_extra:
    cmd += gn_extra
  return run(cmd)


def build(out_dir, build_target, extra_options=None):
  cmd = ['ninja', '-C', out_dir, build_target] + get_ninja_jobs_options()
  if extra_options:
    cmd += extra_options
  return run(cmd)


def install(out_dir):
  cmd = ['build/android/adb_install_apk.py']
  # Propagate PATH to avoid issues with missing tools http://crbug/1217979
  env = {
      'BUILDTYPE': out_dir[4:],
      'PATH': os.environ.get('PATH', '')
  }
  return run(cmd + ['CronetTestInstrumentation.apk'], env=env) or \
      run(cmd + ['ChromiumNetTestSupport.apk'], env=env)


def test(out_dir, extra_options):
  return run([out_dir + '/bin/run_cronet_test_instrumentation_apk'] +
             extra_options)


def unittest(out_dir, extra_options):
  return run([out_dir + '/bin/run_cronet_unittests_android'] +
             extra_options)


def test_ios(out_dir, extra_options):
  return run([out_dir + '/iossim', '-c', quoted_args(extra_options),
             out_dir + '/cronet_test.app'])


def unittest_ios(out_dir, extra_options):
  return run([out_dir + '/iossim', '-c', quoted_args(extra_options),
             out_dir + '/cronet_unittests_ios.app'])


def debug(extra_options):
  return run(['build/android/adb_gdb', '--start',
             '--activity=.CronetTestActivity',
             '--program-name=CronetTest',
             '--package-name=org.chromium.net'] +
             extra_options)


def stack(out_dir):
  return run_shell(
      'adb logcat -d | CHROMIUM_OUTPUT_DIR=' + pipes.quote(out_dir) +
      ' third_party/android_platform/development/scripts/stack')


def use_goma():
  goma_dir = (subprocess.check_output(['goma_ctl', 'goma_dir'])
                        .decode('utf-8')
                        .strip())
  result = run(['goma_ctl', 'ensure_start'])
  if not result:
    return 'use_goma=true goma_dir="' + goma_dir + '" '
  return ''


def get_ninja_jobs_options():
  if use_goma():
    return ["-j1000"]
  return []


def get_default_gn_args(target_os, is_release):
  gn_args = 'target_os="' + target_os + ('" enable_websockets=false '
      'disable_file_support=true '
      'disable_brotli_filter=false '
      'is_component_build=false '
      'use_crash_key_stubs=true '
      'use_partition_alloc=false '
      'include_transport_security_state_preload_list=false ') + use_goma()
  if (is_release):
    gn_args += 'is_debug=false is_official_build=true '
  return gn_args


def get_mobile_gn_args(target_os, is_release):
  return get_default_gn_args(target_os, is_release) + \
      'use_platform_icu_alternatives=true '


def get_ios_gn_args(is_release, bundle_id_prefix, target_cpu):
  print(is_release, bundle_id_prefix, target_cpu)
  return get_mobile_gn_args('ios', is_release) + \
      ('is_cronet_build=true  '
      'enable_remoting=false '
      'ios_app_bundle_id_prefix="%s" '
      'ios_deployment_target="10.0" '
      'enable_dsyms=true '
      'ios_stack_profiler_enabled=false '
      'target_cpu="%s" ') % (bundle_id_prefix, target_cpu)


def get_android_gn_args(is_release):
  return (get_mobile_gn_args('android', is_release) +
          # Keep in sync with //tools/mb/mb_config.pyl cronet_android config.
          'default_min_sdk_version = 19 ' +
          'use_errorprone_java_compiler=true ' +
          'enable_reporting=true ' +
          'use_hashed_jni_names=true ')


def get_mac_gn_args(is_release):
  return get_default_gn_args('mac', is_release) + \
      'disable_histogram_support=true ' + \
      'enable_dsyms=true '


def main():
  is_ios = (sys.platform == 'darwin')
  parser = argparse.ArgumentParser()
  parser.add_argument('command',
                      choices=['gn',
                               'sync',
                               'build',
                               'install',
                               'proguard',
                               'test',
                               'build-test',
                               'unit',
                               'build-unit',
                               'stack',
                               'debug',
                               'build-debug'])
  parser.add_argument('-d', '--out_dir', action='store',
                      help='name of the build directory')
  parser.add_argument('-x', '--x86', action='store_true',
                      help='build for Intel x86 architecture')
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')
  parser.add_argument('-a', '--asan', action='store_true',
                      help='use address sanitizer')
  if is_ios:
    parser.add_argument('-i', '--iphoneos', action='store_true',
                      help='build for physical iphone')
    parser.add_argument('-b', '--bundle-id-prefix', action='store',
                      dest='bundle_id_prefix', default='org.chromium',
                      help='configure bundle id prefix')

  options, extra_options = parser.parse_known_args()
  print(options)
  print(extra_options)

  if is_ios:
    test_target = 'cronet_test'
    unit_target = 'cronet_unittests_ios'
    gn_extra = ['--ide=xcode', '--filters=//components/cronet/*']
    if options.iphoneos:
      gn_args = get_ios_gn_args(options.release, options.bundle_id_prefix,
                                'arm64')
      out_dir_suffix = '-iphoneos'
    else:
      gn_args = get_ios_gn_args(options.release, options.bundle_id_prefix,
                                'x64')
      out_dir_suffix = '-iphonesimulator'
      if options.asan:
        gn_args += 'is_asan=true '
        out_dir_suffix += '-asan'
  else:
    test_target = 'cronet_test_instrumentation_apk'
    unit_target = 'cronet_unittests_android'
    gn_args = get_android_gn_args(
        options.release) + " treat_warnings_as_errors=false "
    gn_extra = []
    out_dir_suffix = ''
    if options.x86:
      gn_args += 'target_cpu="x86" '
      out_dir_suffix = '-x86'
    else:
      gn_args += 'arm_use_neon=false '
    if options.asan:
      # ASAN on Android requires one-time setup described here:
      # https://www.chromium.org/developers/testing/addresssanitizer
      gn_args += 'is_asan=true is_clang=true is_debug=false '
      out_dir_suffix += '-asan'

  if options.release:
    out_dir = 'out/Release' + out_dir_suffix
  else:
    out_dir = 'out/Debug' + out_dir_suffix

  if options.out_dir:
    out_dir = options.out_dir

  if (options.command=='gn'):
    return gn(out_dir, gn_args, gn_extra)
  if (options.command=='sync'):
    return run(['git', 'pull', '--rebase']) or run(['gclient', 'sync'])
  if (options.command=='build'):
    return build(out_dir, test_target, extra_options)
  if (not is_ios):
    if (options.command=='install'):
      return install(out_dir)
    if (options.command=='proguard'):
      return build(out_dir, 'cronet_sample_proguard_apk')
    if (options.command=='test'):
      return install(out_dir) or test(out_dir, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or install(out_dir) or \
          test(out_dir, extra_options)
    if (options.command=='stack'):
      return stack(out_dir)
    if (options.command=='debug'):
      return install(out_dir) or debug(extra_options)
    if (options.command=='build-debug'):
      return build(out_dir, test_target) or install(out_dir) or \
          debug(extra_options)
    if (options.command=='unit'):
      return unittest(out_dir, extra_options)
    if (options.command=='build-unit'):
      return build(out_dir, unit_target) or unittest(out_dir, extra_options)
  else:
    if (options.command=='test'):
      return test_ios(out_dir, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or test_ios(out_dir, extra_options)
    if (options.command=='unit'):
      return unittest_ios(out_dir, extra_options)
    if (options.command=='build-unit'):
      return build(out_dir, unit_target) or unittest_ios(out_dir, extra_options)

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())

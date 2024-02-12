#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Contains general-purpose methods that can be used to execute shell,
GN and Ninja commands.
"""

import subprocess
import os
import re

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

_MB_PATH = os.path.join(REPOSITORY_ROOT, 'tools/mb/mb.py')
_GN_PATH = os.path.join(REPOSITORY_ROOT, 'buildtools/linux64/gn')


def run(command, **kwargs):
  """See the official documentation for subprocess.call.

  Args:
    command (list[str]): command to be executed

  Returns:
    int: the return value of subprocess.call
  """
  print(command, kwargs)
  return subprocess.call(command, **kwargs)


def run_shell(command, extra_options=''):
  """Runs a shell command.

  Runs a shell command with no escaping. It is recommended
  to use `run` instead.
  """
  command = command + ' ' + extra_options
  print(command)
  return os.system(command)


def gn(out_dir, gn_args, gn_extra=None):
  """ Executes `gn gen`.

  Runs `gn gen |out_dir| |gn_args + gn_extra|` which will generate
  a GN configuration that lives under |out_dir|. This is done
  locally on the same chromium checkout.

  Args:
    out_dir (str): Path to delegate to `gn gen`.
    gn_args (str): Args as a string delimited by space.
    gn_extra (str): extra args as a string delimited by space.

  Returns:
    Exit code of running `gn gen` command with argument provided.
  """
  cmd = [_GN_PATH, 'gen', out_dir, '--args=%s' % gn_args]
  if gn_extra:
    cmd += gn_extra
  return run(cmd)


def build(out_dir, build_target, extra_options=None):
  """Runs `ninja build`.

  Runs `ninja -C |out_dir| |build_target| |extra_options|` which will build
  the target |build_target| for the GN configuration living under |out_dir|.
  This is done locally on the same chromium checkout.

  Returns:
    Exit code of running `ninja ..` command with the argument provided.
  """
  cmd = ['ninja', '-C', out_dir, build_target] + _get_ninja_jobs_options()
  if extra_options:
    cmd += extra_options
  return run(cmd)


def android_gn_gen(is_release, target_cpu, out_dir):
  """Runs `gn gen` using Cronet's android gn_args.

  Creates a local GN configuration under |out_dir| with the provided argument
  as input to `get_android_gn_args`, see the documentation of
  `get_android_gn_args` for more information.
  """
  return gn(out_dir, ' '.join(get_android_gn_args(is_release, target_cpu)))


def get_android_gn_args(is_release, target_cpu):
  """Fetches the gn args for a specific builder.

  Returns a list of gn args used by the builders whose target cpu
  is |target_cpu| and (dev or rel) depending on is_release.

  See https://ci.chromium.org/p/chromium/g/chromium.android/console for
  a list of the builders

  Example:

  get_android_gn_args(true, 'x86') -> GN Args for `android-cronet-x86-rel`
  get_android_gn_args(false, 'x86') -> GN Args for `android-cronet-x86-dev`
  """
  group_name = 'chromium.android'
  builder_name = _map_config_to_android_builder(is_release, target_cpu)
  # Ideally we would call `mb_py gen` directly, but we need to filter out the
  # use_remoteexec arg, as that cannot be used in a local environment.
  gn_args = subprocess.check_output(
      ['python3', _MB_PATH, 'lookup', '-m', group_name, '-b',
       builder_name]).decode('utf-8').strip()
  return _filter_gn_args(gn_args.split("\n"))


def _use_goma():
  goma_dir = (subprocess.check_output(['goma_ctl',
                                       'goma_dir']).decode('utf-8').strip())
  result = run(['goma_ctl', 'ensure_start'])
  if not result:
    return 'use_goma=true goma_dir="' + goma_dir + '" '
  return ''


def _get_ninja_jobs_options():
  if _use_goma():
    return ["-j1000"]
  return []


def _map_config_to_android_builder(is_release, target_cpu):
  target_cpu_to_base_builder = {
      'x86': 'android-cronet-x86',
      'x64': 'android-cronet-x64',
      'arm': 'android-cronet-arm',
      'arm64': 'android-cronet-arm64',
      'riscv64': 'android-cronet-riscv64',
  }
  if target_cpu not in target_cpu_to_base_builder:
    raise ValueError('Unsupported target CPU')

  builder_name = target_cpu_to_base_builder[target_cpu]
  if is_release:
    builder_name += '-rel'
  else:
    builder_name += '-dbg'
  return builder_name


def _filter_gn_args(gn_args):
  gn_arg_matcher = re.compile("^.*=.*$")
  # `mb_py lookup` prints out a bunch of metadata lines which we don't
  # care about, we only want the GN args.
  assert len(gn_args) > 4
  actual_gn_args = gn_args[1:-3]
  for line in gn_args:
    if line in actual_gn_args:
      assert gn_arg_matcher.match(line), \
             f'Not dropping {line}, which does not look like a GN arg'
    else:
      assert not gn_arg_matcher.match(line), \
             f'Dropping {line}, which looks like a GN arg'

  return list(filter(lambda string: "remoteexec" not in string, actual_gn_args))

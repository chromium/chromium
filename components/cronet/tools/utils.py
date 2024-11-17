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
import pathlib
import difflib

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

_MB_PATH = os.path.join(REPOSITORY_ROOT, 'tools/mb/mb.py')
GN_PATH = os.path.join(REPOSITORY_ROOT, 'buildtools/linux64/gn')
NINJA_PATH = os.path.join(REPOSITORY_ROOT, 'third_party/ninja/ninja')
_GN_ARG_MATCHER = re.compile("^.*=.*$")


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


def gn(out_dir, gn_args, gn_extra=None, **kwargs):
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
  cmd = [GN_PATH, 'gen', out_dir, '--args=%s' % gn_args]
  if gn_extra:
    cmd += gn_extra
  return run(cmd, **kwargs)


def compare_text_and_generate_diff(generated_text, golden_text,
                                   golden_file_path):
  """
  Compares the generated text with the golden text.

  returns a diff that can be applied with `patch` if exists.
  """
  golden_lines = [line.rstrip() for line in golden_text.splitlines()]
  generated_lines = [line.rstrip() for line in generated_text.splitlines()]
  if golden_lines == generated_lines:
    return None

  expected_path = os.path.relpath(golden_file_path, REPOSITORY_ROOT)

  diff = difflib.unified_diff(
      golden_lines,
      generated_lines,
      fromfile=os.path.join('before', expected_path),
      tofile=os.path.join('after', expected_path),
      n=0,
      lineterm='',
  )

  return '\n'.join(diff)


def read_file(path):
  """Reads a file as a string"""
  return pathlib.Path(path).read_text()


def build(out_dir, build_target, extra_options=None):
  """Runs `ninja build`.

  Runs `ninja -C |out_dir| |build_target| |extra_options|` which will build
  the target |build_target| for the GN configuration living under |out_dir|.
  This is done locally on the same chromium checkout.

  Returns:
    Exit code of running `ninja ..` command with the argument provided.
  """
  cmd = [_NINJA_PATH, '-C', out_dir, build_target]
  if extra_options:
    cmd += extra_options
  return run(cmd)


def build_all(out_dir, build_targets, extra_options=None):
  """Runs `ninja build`.

  Runs `ninja -C |out_dir| |build_targets| |extra_options|` which will build
  the targets |build_targets| for the GN configuration living under |out_dir|.
  This is done locally on the same chromium checkout.

  Returns:
    Exit code of running `ninja ..` command with the argument provided.
  """
  cmd = [NINJA_PATH, '-C', out_dir]
  cmd.extend(build_targets)
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
  return filter_gn_args(gn_args.split("\n"), [])


def get_path_from_gn_label(gn_label: str) -> str:
  """Returns the path part from a GN Label

  GN label consist of two parts, path and target_name, this will
  remove the target name and return the path or throw an error
  if it can't remove the target_name or if it doesn't exist.
  """
  if ":" not in gn_label:
    raise ValueError(f"Provided gn label {gn_label} is not a proper label")
  return gn_label[:gn_label.find(":")]


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


def _should_remove_arg(arg, keys):
  """An arg is removed if its key appear in the list of |keys|"""
  return arg.split("=")[0].strip() in keys


def filter_gn_args(gn_args, keys_to_remove):
  """Returns a list of filtered GN args.

  (1) GN arg's returned must match the regex |_GN_ARG_MATCHER|.
  (2) GN arg's key must not be in |keys_to_remove|.

  Args:
    gn_args: list of GN args.
    keys_to_remove: List of string that will be removed from gn_args.
  """
  filtered_args = []
  for arg in gn_args:
    if _GN_ARG_MATCHER.match(arg) and not _should_remove_arg(
        arg, keys_to_remove):
      filtered_args.append(arg)
  return filtered_args

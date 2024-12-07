#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Driver script to run Cronet gn2bp on Chromium CI builder.

Generate Android.bp files and boringssl dependencies, and then run Copybara
CLI to import code from Chromium to AOSP by generating an AOSP Gerrit CL.

Copybara triggers Android TreeHugger Presubmit on the pending Gerrit CL to
verify Cronet targets can compile with Soong, to provide feedback on whether
the latest Chromium code and unsubmitted generated files can build Cronet
and pass Cronet tests in Android infra. The CL will not be submitted.
"""

import argparse
import hashlib
import os
import pathlib
import subprocess
import sys
import tempfile
import time
from typing import List, Set

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import build.android.gyp.util.build_utils as build_utils  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

_ARCHS = ['x86', 'x64', 'arm', 'arm64', 'riscv64']
_BORINGSSL_PATH = os.path.join(REPOSITORY_ROOT, 'third_party', 'boringssl')
_BORINGSSL_SCRIPT = os.path.join('src', 'util', 'generate_build_files.py')
_COPYBARA_CONFIG_PATH = os.path.join(REPOSITORY_ROOT,
                                     'components/cronet/gn2bp/copy.bara.sky')
_COPYBARA_PATH = os.path.join(REPOSITORY_ROOT,
                              'tools/copybara/copybara/copybara_deploy.jar')
_EXTRA_GN_ARGS = 'is_cronet_for_aosp_build=true'
_GENERATE_BUILD_SCRIPT_PATH = os.path.join(
    REPOSITORY_ROOT,
    'components/cronet/gn2bp/generate_build_scripts_output.py')
_GN2BP_SCRIPT_PATH = os.path.join(REPOSITORY_ROOT,
                                  'components/cronet/gn2bp/gen_android_bp.py')
_GN_PATH = os.path.join(REPOSITORY_ROOT, 'buildtools/linux64/gn')
_JAVA_HOME = os.path.join(REPOSITORY_ROOT, 'third_party', 'jdk', 'current')
_JAVA_PATH = os.path.join(_JAVA_HOME, 'bin', 'java')
_OUT_DIR = os.path.join(REPOSITORY_ROOT, 'out')
_WORKFLOW_NAME = 'import_cronet_to_aosp_gerrit'


def _run_gn2bp(desc_files: Set[tempfile.NamedTemporaryFile],
               skip_build_scripts: bool) -> int:
  """Run gen_android_bp.py to generate Android.bp.gn2bp files."""
  with tempfile.NamedTemporaryFile(mode='w+',
                                   encoding='utf-8') as build_script_output:

    if skip_build_scripts:
      pathlib.Path(build_script_output.name).write_text('{}')
    elif _run_generate_build_scripts(build_script_output.name) != 0:
      raise RuntimeError('Failed to generate build scripts output!')

    base_cmd = [
        sys.executable, _GN2BP_SCRIPT_PATH, '--repo_root', REPOSITORY_ROOT,
        '--build_script_output', build_script_output.name
    ]
    for desc_file in desc_files:
      # desc_file.name represents the absolute path.
      base_cmd += ['--desc', desc_file.name]

    # TODO(crbug.com/378706121): Remove once license generation is fixed.
    base_cmd += ['--no-license']

    return cronet_utils.run(base_cmd)

def _run_generate_build_scripts(output_path: str) -> int:
  """Run generate_build_scripts_output.py.

  Args:
    output_path: Path of the file that will contain the output.
  """
  return cronet_utils.run([
      sys.executable,
      _GENERATE_BUILD_SCRIPT_PATH,
      '--output',
      output_path,
  ])

def _get_args_for_aosp(arch: str) -> List[str]:
  """Get GN args given an architecture."""
  default_args = cronet_utils.get_android_gn_args(True, arch)
  default_args.append(_EXTRA_GN_ARGS)
  return ' '.join(
      cronet_utils.filter_gn_args(default_args,
                                  ['use_remoteexec']))

def _write_desc_json(gn_out_dir: str,
                     temp_file: tempfile.NamedTemporaryFile) -> int:
  """Generate desc json files needed by gen_android_bp.py."""
  return cronet_utils.run([
      _GN_PATH, 'desc', gn_out_dir, '--format=json', '--all-toolchains',
      '//*'], stdout=temp_file)


def _gen_boringssl() -> int:
  """Generate boringssl Android build files."""
  cmd = 'cd {boringssl_path} && python3 {boringssl_script} android'.format(
        boringssl_path=_BORINGSSL_PATH, boringssl_script=_BORINGSSL_SCRIPT)
  return cronet_utils.run(cmd, shell=True)


def _run_copybara_to_aosp(config: str = _COPYBARA_CONFIG_PATH,
                          workflow: str = _WORKFLOW_NAME,
                          copybara_binary: str = _COPYBARA_PATH) -> int:
  """Run Copybara CLI to generate an AOSP Gerrit CL with the generated files.
  Get the commit hash of AOSP `external/cronet` tip of tree to merge into.
  It will print the generated Gerrit url to stdout.
  """
  parent_commit_raw = subprocess.check_output(
      ('git ls-remote '
       'https://android.googlesource.com/platform/external/cronet '
       '| grep "refs/heads/main$" | cut -f 1'), shell=True)
  parent_commit = parent_commit_raw.decode('utf-8').strip('\n')
  print(f'AOSP {parent_commit=}')
  # TODO(crbug.com/349099325): Generate gerrit change id until
  # --gerrit-new-change flag is fixed.
  msg = f'gn2bp{time.time_ns()}'
  change_id = f'I{hashlib.sha1(msg.encode()).hexdigest()}'
  print(f'Generated {change_id=}')
  with tempfile.TemporaryDirectory() as empty_dir:
    return cronet_utils.run([
        _JAVA_PATH,
        '-jar',
        copybara_binary,
        config,
        workflow,
        REPOSITORY_ROOT,
        # We use copybara in merge_import mode to preserve local changes in AOSP
        # that were not upstreamed to Chromium (in practice, that means
        # various additional files such as external/cronet/android/). This means
        # copybara will attempt to do a 3-way merge between:
        #
        #  - Left side: AOSP HEAD
        #  - Right side: final output of this run (including transforms)
        #  - Center: the "baseline"
        #
        # The correct baseline here would be the output of the *previous* run,
        # i.e. the output of the run that was used to produce the current AOSP
        # HEAD. This, however, is not trivial to produce: we'd need to find some
        # way to determine which Chromium commit the previous run used input,
        # then check that out, then run gn2bp on that, etc.
        #
        # Instead, we take a shortcut and use an empty directory as the
        # baseline. This is a clever way to trick the 3-way merge into seeing
        # every file as "new" on both sides. What happens then is if a file is
        # only present on one side then that side is used; and if it's present
        # on both sides then the right side (i.e. the output of this run)
        # always overwrites the left side. This is basically a hacky workaround
        # to ensure copybara never deletes our extra files on the AOSP side.
        #
        # While this kinda works, it has two undesirable properties:
        #  - Any change made in AOSP to a file that also exists in Chromium will
        #    be silently overwritten on import, instead of being 3-way merged.
        #    (In practice this means every change to Chromium code in AOSP MUST
        #    be upstreamed in Chromium before importing, otherwise it will be
        #    reverted on import)
        #  - Files deleted in Chromium will never be deleted in AOSP. More
        #    generally, this approach makes copybara incapable of deleting any
        #    files. (This makes sense, because in this setup copybara has no way
        #    to distinguish between an AOSP-only file and a file that was
        #    deleted from Chromium.)
        #
        # TODO: https://crbug.com/382268057 - improve on the above.
        '--baseline-for-merge-import',
        empty_dir,
        '--change-request-parent',
        parent_commit,
        '--git-push-option',
        'nokeycheck',
        '--git-push-option',
        'uploadvalidator~skip',
        '--ignore-noop',
        '--gerrit-change-id',
        change_id
    ])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--stamp',
                      type=str,
                      help='Path to touch on success',
                      required=True)
  parser.add_argument('--config',
                      type=str,
                      help='Copy.bara.sky file path to run Copybara on',
                      default=_COPYBARA_CONFIG_PATH,
                      required=False)
  parser.add_argument('--workflow',
                      type=str,
                      help='Name of workflow in copy.bara.sky to run',
                      default=_WORKFLOW_NAME,
                      required=False)
  parser.add_argument('--copybara',
                      type=str,
                      help=('Path to copybara executable binary downloaded '
                            'from CIPD'),
                      default=_COPYBARA_PATH,
                      required=False)
  parser.add_argument('--skip_build_scripts',
                      type=bool,
                      help=('Skips building the build_scripts output, '
                            'this should be only used for testing.'))
  args = parser.parse_args()

  try:
    # Create empty temp file for each architecture.
    arch_to_temp_desc_file = {
        arch: tempfile.NamedTemporaryFile(mode="w+", encoding='utf-8')
        for arch in _ARCHS
    }

    for (arch, temp_file) in arch_to_temp_desc_file.items():
        # gn desc behaves completely differently when the output
        # directory is outside of chromium/src, some paths will
        # stop having // in the beginning of their labels
        # eg (//A/B will become A/B), this mostly apply to files
        # that are generated through actions and not targets.
        #
        # This is why the temporary directory has to be generated
        # beneath the repository root until gn2bp is tweaked to
        # deal with this small differences.
      with tempfile.TemporaryDirectory(dir=_OUT_DIR) as gn_out_dir:
        cronet_utils.gn(gn_out_dir, _get_args_for_aosp(arch))
        if _write_desc_json(gn_out_dir, temp_file) != 0:
          # Exit if we failed to generate any of the desc.json files.
          print(f"Failed to generate desc file for arch: {arch}")
          sys.exit(-1)

    res_gn2bp = _run_gn2bp(
        desc_files=arch_to_temp_desc_file.values(),
        skip_build_scripts=args.skip_build_scripts)
    res_boringssl = _gen_boringssl()

    res_copybara = 1
    if res_gn2bp == 0 and res_boringssl == 0:
      # Only run Copybara if all build files generated successfully.
      res_copybara = _run_copybara_to_aosp(
          config=args.config,
          workflow=args.workflow,
          copybara_binary=args.copybara)

  finally:
    for file in arch_to_temp_desc_file.values():
      # Close the temporary files so they can be deleted.
      file.close()

  if res_gn2bp != 0:
    print('Failed to execute gn2bp!')
    sys.exit(-1)
  elif res_boringssl != 0:
    print('Failed to execute boringssl!')
    sys.exit(-1)
  elif res_copybara != 0:
    print('Failed to execute copybara!')
    sys.exit(-1)
  else:
    build_utils.Touch(args.stamp)
    print('Successfully run copybara!')
  return 0


if __name__ == '__main__':
  sys.exit(main())

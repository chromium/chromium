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
import contextlib
import hashlib
import os
import pathlib
import subprocess
import sys
import tempfile
import time
from typing import List, Optional, Set, Tuple

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import build.android.gyp.util.build_utils as build_utils  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

_BORINGSSL_PATH = os.path.join(REPOSITORY_ROOT, 'third_party', 'boringssl')
_BORINGSSL_SCRIPT = os.path.join('src', 'util', 'generate_build_files.py')
_COPYBARA_CONFIG_PATH = os.path.join(REPOSITORY_ROOT,
                                     'components/cronet/gn2bp/copy.bara.sky')
_COPYBARA_PATH = os.path.join(REPOSITORY_ROOT,
                              'tools/copybara/copybara/copybara_deploy.jar')
_GENERATE_BUILD_SCRIPT_PATH = os.path.join(
    REPOSITORY_ROOT,
    'components/cronet/gn2bp/generate_build_scripts_output.py')
_GENERATE_LICENSE_SCRIPT_PATH = os.path.join(
    REPOSITORY_ROOT,
    'components/cronet/license/create_android_metadata_license.py')
_GN2BP_SCRIPT_PATH = os.path.join(REPOSITORY_ROOT,
                                  'components/cronet/gn2bp/gen_android_bp.py')
_JAVA_HOME = os.path.join(REPOSITORY_ROOT, 'third_party', 'jdk', 'current')
_JAVA_PATH = os.path.join(_JAVA_HOME, 'bin', 'java')
_OUT_DIR = os.path.join(REPOSITORY_ROOT, 'out')


class _OptionalExit(contextlib.AbstractContextManager):
  """A context manager wrapper that optionally skips the exit phase of its
  inner context manager."""
  _inner_context_manager: contextlib.AbstractContextManager
  _exit: bool

  def __init__(self, inner_context_manager: contextlib.AbstractContextManager,
               exit: bool):
    self._inner_context_manager = inner_context_manager
    self._exit = exit

  def __enter__(self):
    return self._inner_context_manager.__enter__()

  def __exit__(self, exc_type, exc_val, exc_tb):
    if self._exit:
      return self._inner_context_manager.__exit__(exc_type, exc_val, exc_tb)


def _run_license_generation() -> int:
  return cronet_utils.run(["python3", _GENERATE_LICENSE_SCRIPT_PATH])


def _run_gn2bp(desc_files: Set[tempfile.NamedTemporaryFile],
               skip_build_scripts: bool, delete_temporary_files: bool) -> int:
  """Run gen_android_bp.py to generate Android.bp.gn2bp files."""
  with tempfile.NamedTemporaryFile(
      mode='w+', encoding='utf-8',
      delete=delete_temporary_files) as build_script_output:

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

    base_cmd += ["--license"]
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

def _write_desc_json(gn_out_dir: str,
                     temp_file: tempfile.NamedTemporaryFile) -> int:
  """Generate desc json files needed by gen_android_bp.py."""
  return cronet_utils.run([
      cronet_utils.GN_PATH, 'desc', gn_out_dir, '--format=json',
      '--all-toolchains', '//*'
  ],
                          stdout=temp_file)


def _gen_boringssl() -> int:
  """Generate boringssl Android build files."""
  cmd = 'cd {boringssl_path} && python3 {boringssl_script} android'.format(
        boringssl_path=_BORINGSSL_PATH, boringssl_script=_BORINGSSL_SCRIPT)
  return cronet_utils.run(cmd, shell=True)


def _run_copybara_to_aosp(config: str = _COPYBARA_CONFIG_PATH,
                          copybara_binary: str = _COPYBARA_PATH,
                          git_url_and_branch: Optional[Tuple[str, str]] = None,
                          regenerate_consistency_file: bool = False) -> int:
  """Run Copybara CLI to generate an AOSP Gerrit CL with the generated files.
  Get the commit hash of AOSP `external/cronet` tip of tree to merge into.
  It will print the generated Gerrit url to stdout.
  """
  if not git_url_and_branch:
    parent_commit_raw = subprocess.check_output(
        ('git ls-remote '
         'https://android.googlesource.com/platform/external/cronet '
         '| grep "refs/heads/main$" | cut -f 1'),
        shell=True)
    parent_commit = parent_commit_raw.decode('utf-8').strip('\n')
    print(f'AOSP {parent_commit=}')
    # TODO(crbug.com/349099325): Generate gerrit change id until
    # --gerrit-new-change flag is fixed.
    msg = f'gn2bp{time.time_ns()}'
    change_id = f'I{hashlib.sha1(msg.encode()).hexdigest()}'
    print(f'Generated {change_id=}')
  return cronet_utils.run([
      _JAVA_PATH,
      '-jar',
      copybara_binary,
      config,
      "import_cronet_to_aosp_gerrit"
      if git_url_and_branch is None else "import_cronet_to_git_branch",
      REPOSITORY_ROOT,
      '--ignore-noop',
      *(('--change-request-parent', parent_commit, '--git-push-option',
         'nokeycheck', '--git-push-option', 'uploadvalidator~skip',
         '--gerrit-change-id', change_id) if git_url_and_branch is None else
        ('--git-destination-url', git_url_and_branch[0],
         '--git-destination-push', git_url_and_branch[1])),
      # We can't use the copybara `regenerate` subcommand because it doesn't
      # support folder origins. See https://crbug.com/391331930.
      *(('--disable-consistency-merge-import', 'true',
         '--baseline-for-merge-import',
         REPOSITORY_ROOT) if regenerate_consistency_file else ())
  ])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--stamp', type=str, help='Path to touch on success')
  parser.add_argument('--config',
                      type=str,
                      help='Copy.bara.sky file path to run Copybara on',
                      default=_COPYBARA_CONFIG_PATH,
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
  parser.add_argument('--skip-copybara',
                      action='store_true',
                      help=("Only generate the build files - do not run "
                            "copybara afterwards. This is useful if you only "
                            "want to take a look at the generated files "
                            "without doing an actual import."))
  parser.add_argument('--git-url-and-branch',
                      type=str,
                      help=("Git URL and branch to push to. If not specified, "
                            "creates an AOSP Gerrit CL. This option is useful "
                            "to push to a local git repo for manual testing, "
                            "for example: "
                            "file:////home/foo/aosp/external/cronet mybranch"),
                      nargs=2)
  parser.add_argument('--keep-temporary-files',
                      action='store_true',
                      help=("Don't clean up temporary files. Useful for "
                            "troubleshooting."))
  parser.add_argument('--regenerate-consistency-file',
                      action='store_true',
                      help=("Ask copybara to ignore the existing merge import "
                            "consistency file and generate a new one. Note for "
                            "this to work the script must be run from the same "
                            "origin as the one that was used for the last "
                            "import into the destination; in other words, you "
                            "must re-import the exact same Cronet version that "
                            "is currently in the destination."))
  args = parser.parse_args()
  run_copybara = not args.skip_copybara
  delete_temporary_files = not args.keep_temporary_files

  try:
    # Create empty temp file for each architecture.
    arch_to_temp_desc_file = {
        arch:
        tempfile.NamedTemporaryFile(mode="w+",
                                    encoding='utf-8',
                                    delete=delete_temporary_files)
        for arch in cronet_utils.ARCHS
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
      with _OptionalExit(tempfile.TemporaryDirectory(dir=_OUT_DIR),
                         exit=delete_temporary_files) as gn_out_dir:
        cronet_utils.gn(gn_out_dir, ' '.join(cronet_utils.get_gn_args_for_aosp(arch)))
        if _write_desc_json(gn_out_dir, temp_file) != 0:
          # Exit if we failed to generate any of the desc.json files.
          print(f"Failed to generate desc file for arch: {arch}")
          sys.exit(-1)

    res_license_generation = _run_license_generation()
    res_gn2bp = _run_gn2bp(desc_files=arch_to_temp_desc_file.values(),
                           skip_build_scripts=args.skip_build_scripts,
                           delete_temporary_files=delete_temporary_files)
    res_boringssl = _gen_boringssl()

    res_copybara = 1
    if run_copybara and res_gn2bp == 0 and res_boringssl == 0 and res_license_generation == 0:
      # Only run Copybara if all build files generated successfully.
      res_copybara = _run_copybara_to_aosp(
          config=args.config,
          copybara_binary=args.copybara,
          git_url_and_branch=args.git_url_and_branch,
          regenerate_consistency_file=args.regenerate_consistency_file)

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
  elif res_license_generation != 0:
    print('Failed to generate license data!')
    sys.exit(-1)
  elif run_copybara and res_copybara != 0:
    print('Failed to execute copybara!')
    sys.exit(-1)
  else:
    if args.stamp is not None:
      build_utils.Touch(args.stamp)
    print('Success!')
  return 0


if __name__ == '__main__':
  sys.exit(main())

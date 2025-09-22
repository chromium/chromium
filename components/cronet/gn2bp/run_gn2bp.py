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
import multiprocessing.dummy
import json
import os
import pathlib
import string
import subprocess
import sys
import tempfile
import textwrap
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
    REPOSITORY_ROOT, 'components/cronet/gn2bp/generate_build_scripts_output.py')
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
               do_exit: bool):
    self._inner_context_manager = inner_context_manager
    self._exit = do_exit

  def __enter__(self):
    return self._inner_context_manager.__enter__()

  def __exit__(self, exc_type, exc_val, exc_tb):
    if self._exit:
      return self._inner_context_manager.__exit__(exc_type, exc_val, exc_tb)
    return None


def _get_version_string() -> str:
  version = ''
  chrome_version_file_path = os.path.join(REPOSITORY_ROOT, 'chrome', 'VERSION')
  for version_component in cronet_utils.read_file(
      chrome_version_file_path).split('\n'):
    if not version_component:
      # Ignore empty lines
      continue
    if version:
      # Only subsequent version components should be split by dots
      version += '.'
    version += version_component.split('=')[1]
  return version


def _run_license_generation():
  cronet_utils.run(["python3", _GENERATE_LICENSE_SCRIPT_PATH])


def _run_gn2bp(desc_files: Set[tempfile.NamedTemporaryFile],
               skip_build_scripts: bool, delete_temporary_files: bool,
               channel: str) -> int:
  """Run gen_android_bp.py to generate Android.bp.gn2bp files."""
  with tempfile.NamedTemporaryFile(
      mode='w+', encoding='utf-8',
      delete=delete_temporary_files) as build_script_output:

    if skip_build_scripts:
      pathlib.Path(build_script_output.name).write_text('{}')
    else:
      _run_generate_build_scripts(build_script_output.name)

    base_cmd = [
        sys.executable, _GN2BP_SCRIPT_PATH, '--repo_root', REPOSITORY_ROOT,
        '--build_script_output', build_script_output.name
    ]
    for desc_file in desc_files:
      # desc_file.name represents the absolute path.
      base_cmd += ['--desc', desc_file.name]

    base_cmd += ["--license"]
    base_cmd += ["--channel", channel]
    cronet_utils.run(base_cmd)


def _run_generate_build_scripts(output_path: str):
  """Run generate_build_scripts_output.py.

  Args:
    output_path: Path of the file that will contain the output.
  """
  cronet_utils.run([
      sys.executable,
      _GENERATE_BUILD_SCRIPT_PATH,
      '--output',
      output_path,
  ])


def _write_desc_json(gn_out_dir: str, temp_file: tempfile.NamedTemporaryFile):
  """Generate desc json files needed by gen_android_bp.py."""
  cronet_utils.run(
      [cronet_utils.GN_PATH, 'desc', gn_out_dir, '--format=json', '//*'],
      stdout=temp_file)


def _gen_extras_bp(import_channel: str):
  """Generate Android.extras.bp."""
  extras_androidbp_template_path = os.path.join(REPOSITORY_ROOT, 'components',
                                                'cronet', 'gn2bp', 'templates',
                                                'Android.extras.bp.template')
  extras_androidbp_template_contents = cronet_utils.read_file(
      extras_androidbp_template_path)
  extras_androidbp_path = os.path.join(REPOSITORY_ROOT,
                                       'Android.extras.bp.gn2bp')
  cronet_utils.write_file(
      extras_androidbp_path,
      string.Template(extras_androidbp_template_contents).substitute(
          GN2BP_MODULE_PREFIX=f'{import_channel}_cronet_'))


def _gen_androidtest_xml():
  """Generate AndroidTest.xml, required to run test in Android."""
  androidtest_xml_template_path = os.path.join(REPOSITORY_ROOT, 'components',
                                               'cronet', 'gn2bp', 'templates',
                                               'AndroidTest.xml.template')
  androidtest_xml_template_contents = cronet_utils.read_file(
      androidtest_xml_template_path)
  androidtest_xml_path = os.path.join(REPOSITORY_ROOT, 'AndroidTest.xml')
  cronet_utils.write_file(androidtest_xml_path,
                          androidtest_xml_template_contents)

def _gen_boringssl(import_channel: str):
  """Generate boringssl Android build files."""
  module_prefix = f'{import_channel}_cronet_'
  boringssl_androidbp_template_path = os.path.join(
      REPOSITORY_ROOT, 'components', 'cronet', 'gn2bp', 'templates',
      'boringssl_Android.bp.template')
  boringssl_androidbp_template_contents = cronet_utils.read_file(
      boringssl_androidbp_template_path)
  boringssl_androidbp_path = os.path.join(_BORINGSSL_PATH, 'Android.bp.gn2bp')
  cronet_utils.write_file(
      boringssl_androidbp_path,
      string.Template(boringssl_androidbp_template_contents).substitute(
          GN2BP_IMPORT_CHANNEL=import_channel,
          GN2BP_MODULE_PREFIX=module_prefix))
  cmd = f'cd {_BORINGSSL_PATH} && python3 {_BORINGSSL_SCRIPT} --target-prefix={module_prefix} android'
  cronet_utils.run(cmd, shell=True)


def _wait_and_fail_if_not_presubmit_verified(change_id: str):
  gerrit_client_path = os.path.join(REPOSITORY_ROOT, 'third_party',
                                    'depot_tools', 'gerrit_client.py')
  while True:
    with tempfile.NamedTemporaryFile(mode="w+", encoding='utf-8',
                                     delete=True) as gerrit_change_labels_file:
      cronet_utils.run([
          gerrit_client_path, 'changes',
          '--host=https://googleplex-android-review.googlesource.com',
          '--project=platform/external/cronet', f'--query={change_id}', '-o',
          'LABELS', f'--json={gerrit_change_labels_file.name}'
      ])
      cronet_change_labels = json.loads(
          cronet_utils.read_file(gerrit_change_labels_file.name))
      presubmit_verified_entries = cronet_change_labels[0]['labels'][
          'Presubmit-Verified']
      for key in presubmit_verified_entries:
        if key in ('rejected', 'disliked'):
          raise RuntimeError(
              'Presubmit failed, check the Android CL for more info')
        if key in ('approved', 'recommended'):
          return
      print(
          f'Still waiting for Presubmit-Verified: {presubmit_verified_entries}')
      time.sleep(60 * 5)  # 5 mins


def _run_copybara_to_aosp(config: str, copybara_binary: str,
                          git_url_and_branch: Optional[Tuple[str, str]],
                          regenerate_consistency_file: bool,
                          import_channel: str,
                          wait_for_presubmit_verified: bool):
  """Run Copybara CLI to generate an AOSP Gerrit CL with the generated files.
  Get the commit hash of AOSP `external/cronet` tip of tree to merge into.
  It will print the generated Gerrit url to stdout.
  """
  msg = f'gn2bp{time.time_ns()}'
  change_id = f'I{hashlib.sha1(msg.encode()).hexdigest()}'
  print(f'Generated {change_id=}')

  version = _get_version_string()
  commit_hash = cronet_utils.run_and_get_stdout(['git', 'rev-parse', 'HEAD'])
  commit_date = cronet_utils.run_and_get_stdout(
      ['git', 'show', '--pretty=format:%ci', '--no-patch'])
  swarming_task_id = os.environ.get('SWARMING_TASK_ID')
  commit_message = textwrap.dedent(f"""\
      Import Cronet {commit_hash[:8]} ({version}) into {import_channel}

      Chromium commit hash: {commit_hash}
      Chromium commit date: {commit_date}
      Chromium version: {version}

      """)
  if not os.environ.get('SWARMING_BOT_ID', '').startswith('luci-chrome-ci-'):
    # This is not ideal, but we don't have a better signal to tell if gn2bp is
    # running in CI or somewhere else.
    #
    # Chromium CQ checks for this string in code, so this must be split to land
    # the change.
    prefix = 'DO NOT ' + 'SUBMIT'
    commit_message += textwrap.dedent(f"""\
        {prefix}: This import was not generated by Chromium's CI, as such
        it might contain unreviewed changes on top of the aforementioned commit.

        """)
  if swarming_task_id:
    commit_message += textwrap.dedent(f"""\
        This CL was autogenerated by the following Chromium bot run:
        https://luci-milo.appspot.com/swarming/task/{swarming_task_id}?server=chrome-swarming.appspot.com

        """)
  commit_message += textwrap.dedent(f"""\
      This CL can be reproduced by running the following command:
      gclient config --spec 'solutions = [
      {{
          "name": "src",
          "url": "https://chromium.googlesource.com/chromium/src.git",
          "managed": False,
          "custom_deps": {{}},
          "custom_vars": {{
            "checkout_copybara": True,
          }},
        }},
      ]
      target_os = ["android"]
      ' && gclient sync --rev={commit_hash} && cd src
      && vpython3 components/cronet/gn2bp/run_gn2bp.py --channel={import_channel}

      The state of Chromium, for the commit being imported, can be browsed at:
      https://chromium.googlesource.com/chromium/src/+/{commit_hash}

      NO_IFTTT=Imported from Chromium.
      """)
  additional_parameters = [
      '--ignore-noop',
      '--force-message',
      commit_message,
  ]

  target_workflow = None
  if git_url_and_branch:
    target_workflow = f'{import_channel}_import_cronet_to_git_branch'
    additional_parameters.extend([
        '--git-destination-url',
        git_url_and_branch[0],
        '--git-destination-push',
        git_url_and_branch[1],
    ])
  else:
    target_workflow = f'{import_channel}_import_cronet_to_aosp_gerrit'
    additional_parameters.extend([
        '--git-push-option',
        'nokeycheck',
        '--git-push-option',
        'uploadvalidator~skip',
        '--gerrit-change-id',
        change_id,
    ])
  if regenerate_consistency_file:
    # We can't use the copybara `regenerate` subcommand because it doesn't
    # support folder origins. See https://crbug.com/391331930.
    additional_parameters.extend([
        '--disable-consistency-merge-import',
        'true',
        '--baseline-for-merge-import',
        REPOSITORY_ROOT,
    ])

  cronet_utils.run([
      _JAVA_PATH, '-jar', copybara_binary, config, target_workflow,
      REPOSITORY_ROOT
  ] + additional_parameters)

  if wait_for_presubmit_verified and not git_url_and_branch:
    _wait_and_fail_if_not_presubmit_verified(change_id)



def _fill_desc_file_for_arch(arch, desc_file, delete_temporary_files):
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
                     do_exit=delete_temporary_files) as gn_out_dir:
    cronet_utils.gn(gn_out_dir,
                    ' '.join(cronet_utils.get_gn_args_for_aosp(arch)))
    _write_desc_json(gn_out_dir, desc_file)


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
  parser.add_argument('--channel',
                      help='The channel this execution of gn2bp is targeting.',
                      type=str,
                      choices=['tot', 'stable'],
                      default='tot')
  parser.add_argument(
      '--wait-for-presubmit-verified',
      help=
      'Whether the script should wait for presubmit verified after uploading a CL to Android',
      action='store_true')
  args = parser.parse_args()
  delete_temporary_files = not args.keep_temporary_files

  if not args.skip_copybara and os.listdir(
      os.path.join(REPOSITORY_ROOT, 'clank')):
    raise RuntimeError(
        'gn2bp should not be run with an internal code checkout, as copybara'
        ' may end up leaking internal code to the destination')

  try:
    arch_to_desc_file = {
        arch:
        tempfile.NamedTemporaryFile(mode="w+",
                                    encoding='utf-8',
                                    delete=delete_temporary_files)
        for arch in cronet_utils.ARCHS
    }
    with multiprocessing.dummy.Pool(len(arch_to_desc_file.items())) as pool:
      results = [
          pool.apply_async(_fill_desc_file_for_arch,
                           (arch, desc_file, delete_temporary_files))
          for (arch, desc_file) in arch_to_desc_file.items()
      ]
      for result in results:
        # We don't care about result, since there isn't one. This is only
        # needed to re-raises failures raised by _fill_desc_file_for_arch,
        # if any.
        result.get()

    _run_license_generation()
    _run_gn2bp(desc_files=arch_to_desc_file.values(),
               skip_build_scripts=args.skip_build_scripts,
               delete_temporary_files=delete_temporary_files,
               channel=args.channel)
    _gen_boringssl(args.channel)
    _gen_extras_bp(args.channel)
    _gen_androidtest_xml()

    if not args.skip_copybara:
      _run_copybara_to_aosp(
          config=args.config,
          copybara_binary=args.copybara,
          git_url_and_branch=args.git_url_and_branch,
          regenerate_consistency_file=args.regenerate_consistency_file,
          import_channel=args.channel,
          wait_for_presubmit_verified=args.wait_for_presubmit_verified)

  finally:
    for file in arch_to_desc_file.values():
      # Close the temporary files so they can be deleted.
      file.close()

  if args.stamp is not None:
    build_utils.Touch(args.stamp)
  print('Success!')
  return 0


if __name__ == '__main__':
  sys.exit(main())

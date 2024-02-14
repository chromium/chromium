#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""check_cronet_dependencies.py - Keep track of Cronet's dependencies."""

import argparse
import os
import subprocess
import sys
import tempfile
from typing import List, Set

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import build.android.gyp.util.build_utils as build_utils  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

_THIRD_PARTY_STR = 'third_party/'
_GN_PATH = os.path.join(REPOSITORY_ROOT, 'buildtools/linux64/gn')


def _get_current_gn_args() -> List[str]:
  """Returns the GN args in the current working directory"""
  return subprocess.check_output(["cat", "args.gn"]).decode('utf-8').split("\n")


def normalize_third_party_dep(dependency: str) -> str:
  """Normalizes a GN label that includes `third_party` string

  Required because Chromium allows multiple libraries to live under the
  same third_party directory (eg: `third_party/android_deps` contains
  more than a single library), In order to decrease the failure rate
  each time a dependency is added, normalize the `third_party` paths
  to its root.

  If more than one `third_party` string appears in the GN label, the
  last one is picked for normalization. See examples below:

  * "//third_party/foo" -> "//third_party/foo"
  * "//third_party/foo/bar" -> "//third_party/foo"
  * "//third_party/foo/bar/X" -> "//third_party/foo"
  * "//third_party/foo/third_party/bar" -> "//third_party/foo/third_party/bar"

  Args:
    dependency: GN label that represents relative path to a dependency.

  Raises:
    ValueError: Raised if the dependency is not a third_party dependency.

  Returns:
    The normalized third_party path.
  """
  if _THIRD_PARTY_STR not in dependency:
    raise ValueError('Dependency is not a third_party dependency')
  root_end_index = dependency.rfind(_THIRD_PARTY_STR) + len(_THIRD_PARTY_STR)
  dependency_name_end_index = dependency.find("/", root_end_index)
  if dependency_name_end_index == -1:
    return dependency
  return dependency[:dependency_name_end_index]


def _get_transitive_deps_from_root_targets(out_dir: str,
                                           gn_targets: List[str]) -> Set[str]:
  """Executes gn desc |out_dir| |gn_target| deps --all for each gn target"""
  all_deps = set()
  for gn_target in gn_targets:
    all_deps.update(
        subprocess.check_output(
            [_GN_PATH, "desc", out_dir, gn_target, "deps",
             "--all"]).decode("utf-8").split("\n"))
  return all_deps


def normalize_and_dedup_deps(deps: Set[str]) -> Set[str]:
  """Deduplicate after normalizing third_party dependencies

  This process involve the following steps:

  (1) Remove the target name from the gn label to retrieve
  the proper path.
  (2) If the gn label involves a third_party dependency then
  normalize it according to |normalize_third_party_dep|.
  (3) Add the final path after processing to the set.

  AndroidX dependencies are a special case and they don't go
  through any processing, they are added as is.

  Args:
    deps: A set of all the dependencies.

  Returns:
    A sorted collection of normalized deps.
  """
  cleaned_deps = set()
  for dep in deps:
    if not dep:
      # Ignore empty lines.
      continue

    if dep.startswith("//third_party/androidx:") and dep.endswith("_java"):
      # We treat androidx dependency differently because
      # Cronet MUST NOT depend on any androidx dependency except
      # androidx_annotations which is compile-time only. This is
      # needed because this is one of mainline restrictions.
      # Java/Android targets in GN must end with _java, this is needed
      # so we don't bloat the dependencies file with auto-generated targets.
      # (eg: androidx_annotation_annotation_java__assetres)

      # Don't do any cleaning, add the exact GN label to the dependencies.
      cleaned_deps.add(dep)
    else:
      dep = cronet_utils.get_path_from_gn_label(dep)
      if _THIRD_PARTY_STR in dep:
        cleaned_deps.add(normalize_third_party_dep(dep))
      else:
        cleaned_deps.add(dep)
  return sorted(cleaned_deps)


def main():
  parser = argparse.ArgumentParser(
      prog='Check cronet dependencies',
      description=
      "Checks whether Cronet's current dependencies match the known ones.")
  parser.add_argument(
      '--root-deps',
      nargs="+",
      help="""Those are the root dependencies which the script will
      use to find the closure of all transitive dependencies.""",
      required=True,
  )
  parser.add_argument(
      '--old-dependencies',
      type=str,
      help='Relative path to file that contains the old dependencies',
      required=True,
  )
  parser.add_argument(
      '--stamp',
      type=str,
      help='Path to touch on success',
  )
  args = parser.parse_args()
  gn_args = _get_current_gn_args()
  # Generate a new GN output directory in order
  # not to mess with the current one.
  with tempfile.TemporaryDirectory() as tmp_dir_name:
    if cronet_utils.gn(tmp_dir_name, " ".join(gn_args)) != 0:
      print("Failed to execute `gn gen` in a temporary directory")
      return -1

    final_deps = normalize_and_dedup_deps(
        _get_transitive_deps_from_root_targets(tmp_dir_name, args.root_deps))
    golden_deps = cronet_utils.read_file(args.old_dependencies).split("\n")
    if not all(dep in golden_deps for dep in final_deps):
      # Only generate this text if we found a new dependency
      # that does not exist in the golden text. This means
      # that we will know not if a dependency gets removed
      # as we don't care about that scenario and we don't
      # want to block people while cleaning-up code.
      print("""
Cronet Dependency check has failed. Please re-generate the golden file:
#######################################################
#                                                     #
#      Run the command below to generate the file     #
#                                                     #
#######################################################

########### START ###########
patch -p1 << 'END_DIFF'
%s
END_DIFF
############ END ############
""" % cronet_utils.compare_text_and_generate_diff(
          '\n'.join(final_deps), cronet_utils.read_file(args.old_dependencies),
          args.old_dependencies))
      return -1

    build_utils.Touch(args.stamp)
    return 0


if __name__ == '__main__':
  sys.exit(main())

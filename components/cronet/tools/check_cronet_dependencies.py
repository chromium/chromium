#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""check_cronet_dependencies.py - Keep track of Cronet's dependencies."""

import argparse
import os
import subprocess
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position


def normalize_third_party_dep(dependency):
  third_party_str = 'third_party/'
  if third_party_str not in dependency:
    raise ValueError('Dependency is not third_party dependency')
  root_end_index = dependency.rfind(third_party_str) + len(third_party_str)
  dependency_name_end_index = dependency.find("/", root_end_index)
  if dependency_name_end_index == -1:
    return dependency
  return dependency[:dependency_name_end_index]


def dedup_third_party_deps_internal(dependencies, root_deps=None):
  if root_deps is None:
    root_deps = set()
  deduped_deps = []
  for dependency in dependencies:
    # crbug.com(1406537): `gn desc deps` can spit out non-deps stuff if it finds unknown
    # GN args
    if not dependency or not dependency.startswith('//'):
      continue
    if dependency[-1] == '/':
      raise ValueError('Dependencies must not have a trailing forward slash')

    if 'third_party/' not in dependency:
      # We don't apply any filtering to non third_party deps.
      deduped_deps.append(dependency)
      continue

    # Take the last occurrence to consider //third_party/foo and
    # //third_party/foo/third_party/bar as two distinct dependencies.
    third_party_dep_segments = dependency.split('third_party/')
    third_party_dep = third_party_dep_segments[-1]
    if '/' not in third_party_dep:
      # Root dependencies are always unique.
      # Note: We append the amount of splits to differentiate between
      # //third_party/foo and //third_party/bar/third_party/foo.
      root_dep = str(len(third_party_dep_segments)) + third_party_dep
      root_deps.add(root_dep)
      deduped_deps.append(normalize_third_party_dep(dependency))
    else:
      third_party_dep_root = (str(len(third_party_dep_segments)) +
                              third_party_dep.split('/')[0])
      if third_party_dep_root not in root_deps:
        root_deps.add(third_party_dep_root)
        deduped_deps.append(normalize_third_party_dep(dependency))
  return (deduped_deps, root_deps)


def dedup_third_party_deps(old_dependencies, new_dependencies):
  """Maintains only a single target for each third_party dependency."""
  (_, root_deps) = dedup_third_party_deps_internal(old_dependencies)
  (deduped_deps, _) = dedup_third_party_deps_internal(new_dependencies,
                                                      root_deps=root_deps)
  return '\n'.join(deduped_deps)


def main():
  parser = argparse.ArgumentParser(
      prog='Check cronet dependencies',
      description=
      "Checks whether Cronet's current dependencies match the known ones.")
  parser.add_argument(
      '--new_dependencies_script',
      type=str,
      help='Relative path to the script that outputs new dependencies',
      required=True,
  )
  parser.add_argument(
      '--old_dependencies',
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

  new_dependencies = subprocess.check_output(
      [args.new_dependencies_script, args.old_dependencies]).decode('utf-8')
  with open(args.old_dependencies, 'r') as f:
    new_dependencies = dedup_third_party_deps(f.read().splitlines(),
                                              new_dependencies.splitlines())
  if new_dependencies != '':
    print('New dependencies detected:')
    print(new_dependencies)
    print('Please update: ' + args.old_dependencies)
    sys.exit(-1)
  else:
    build_utils.Touch(args.stamp)


if __name__ == '__main__':
  main()

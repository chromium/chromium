#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages test dependencies as tar.gz file."""

import argparse
import json
import logging
import os
import sys
import tarfile


parser = argparse.ArgumentParser(
    description='Package test dependencies as tar.gz files.')
parser.add_argument('--output', required=True,
                    help='Full path to the output file.')
parser.add_argument('--deps_list_path', required=True,
                    help='Full path to the json dependencies file.')
parser.add_argument('--exclude_deps', required=False,
                    default='',
                    help=('Comma separated list of dependencies to exclude'
                          ' from tar.gz file.'))
parser.add_argument('--additional_deps', required=False,
                    default='',
                    help=('Comma separated list of additional deps'
                          ' to include in tar.gz.'))


def read_dependencies(file_path):
  """Reads a json file and creates an iterable of unique dependencies.

  Args:
    file_path: The path to the runtime dependencies file.

  Returns:
    An iterable with unique dependencies.
  """
  deps = None
  with open(file_path) as deps_file:
    deps = json.load(deps_file)
  deps_set = set()
  for _, dep_list in deps.items():
    deps_set.update(dep_list)
  return deps_set


def filter_dependencies(dependencies, filters):
  """Filters out dependencies from a dependencies iterable.

  Args:
    dependencies: An iterable with the full list of dependencies.
    filters: A list of dependencies to remove.

  Returns:
    An iterable with the filtered dependencies.
  """
  filters_list = filters.strip(',').split(',')
  logging.info('Filtering: %s', filters_list)
  filtered_deps = set()
  for dep in dependencies:
    norm_dep = os.path.normpath(dep)
    if not any(norm_dep.startswith(f) for f in filters_list):
      filtered_deps.add(norm_dep)
  return filtered_deps


def create_tarfile(output_path, dependencies):
  """Creates a tar.gz file and saves it to output_path.

  Args:
    output_path: A string with the path to where tar.gz file will be saved to.
    dependencies: An iterable with file/folders test dependencies.
  """
  total_deps = len(dependencies)
  if total_deps < 1:
    logging.error('There are no dependencies to archive')
    sys.exit(1)
  step = (total_deps / 10) or 1
  logging.info('Adding %s files', total_deps)
  with tarfile.open(output_path, 'w:gz') as tar_file:
    for idx, dep in enumerate(dependencies):
      dep = os.path.normpath(dep)
      archive_name = os.path.join('fuchsia/release', dep)
      archive_name = os.path.normpath(archive_name)
      tar_file.add(dep, arcname=archive_name)
      if idx % step == 0 or idx == (total_deps - 1):
        logging.info('Progress: %s percent', int(round(100.0/total_deps * idx)))


def main():
  logging.basicConfig(level=logging.INFO)
  args = parser.parse_args()
  dependencies = read_dependencies(args.deps_list_path)
  if args.additional_deps:
    to_include = args.additional_deps.strip(',').split(',')
    logging.info('Including: %s', to_include)
    dependencies.update(to_include)
  if args.exclude_deps:
    dependencies = filter_dependencies(dependencies, args.exclude_deps)
  create_tarfile(args.output, dependencies)


if __name__ == '__main__':
  main()

#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script to generate unit test lists for the Chromecast build scripts.
"""

import glob
import json
import optparse
import os
import sys


def GetTestNames(test_files_dir):
  """Returns test names specified in the *.tests files in |test_files_dir|."""
  test_files = sorted(glob.glob(test_files_dir + "/*.tests"))
  test_names = set()
  for test_filename in test_files:
    with open(test_filename, "r") as test_file:
      for test_file_line in test_file:
        # Binary name may be a simple test target (cast_net_unittests) or be a
        # qualified gyp path (../base.gyp:base_unittests).
        test_name = test_file_line.split(":")[-1].strip()
        test_names.add(test_name)
  return test_names


def GetTestFilters(test_files_dir, test_names, include_filters):
  """Returns filters specified in the *.filters files in |test_files_dir|."""
  # GYP targets may provide a numbered priority for the filename. Sort to
  # use that priority.
  filter_files = sorted(glob.glob(test_files_dir + "/*.filters"))
  test_filters = {}
  if include_filters:
    for filter_filename in filter_files:
      with open(filter_filename, "r") as filter_file:
        for filter_line in filter_file:
          (test_name, test_filter) = filter_line.strip().split(" ", 1)

          if test_name not in test_names:
            raise Exception("Filter found for unknown target: " + test_name)

          # Note: This may overwrite a previous rule. This is okay, since higher
          # priority files are evaluated after lower priority files.
          test_filters[test_name] = test_filter

  return test_filters


def CombineList(test_files_dir, list_output_file, include_filters,
                additional_runtime_options):
  """Writes a unit test file in a format compatible for Chromecast scripts.

  If include_filters is True, uses filters to create a test runner list
  and also include additional options, if any.
  Otherwise, creates a list only of the tests to build.

  Args:
    test_files_dir: Path to the intermediate directory containing tests/filters.
    list_output_file: Path to write the unit test file out to.
    include_filters: Whether or not to include the filters when generating
        the test list.
    additional_runtime_options: Arguments to be applied to all tests.  These are
        applied before filters (so test-specific filters take precedence).

  Raises:
    Exception: if filter is found for an unknown target.
  """
  test_names = GetTestNames(test_files_dir)
  test_filters = GetTestFilters(test_files_dir, test_names, include_filters)
  test_commands = [
      "{} {} {}".format(test_name,
                        additional_runtime_options or "",
                        test_filters.get(test_name, ""))
      for test_name in test_names
  ]

  with open(list_output_file, "w") as f:
    f.write("\n".join(sorted(test_commands)))


def CombineRuntimeDeps(test_files_dir, deps_output_file):
  """Writes a JSON file that lists the runtime dependecies for each test.

  The output will consist of a JSON dictionary where the keys are names of the
  unittests and the values are arrays of files and directories needed at runtime
  by the unittest. Of note, the unittest itself is always listed as a runtime
  dependency of itself.

  The paths are all relative to the root output directory (where the unittest
  binaries live).

  {
    "base_unittests": ["./base_unittests", "../../base/test/data/"],
    "cast_media_unittests": [...],
    ...
  }

  Args:
    test_files_dir: path to the intermediate directory containing the invidual
        runtime deps files.
    deps_output_file: Path to write the JSON file out to.
  """
  test_names = GetTestNames(test_files_dir)
  runtime_deps = {}
  runtime_deps_dir = os.path.join(test_files_dir, "runtime_deps")
  for runtime_deps_file in glob.glob(runtime_deps_dir + "/*_runtime_deps.txt"):
    test_name = os.path.basename(runtime_deps_file).replace(
        "_runtime_deps.txt", "")
    if test_name not in test_names:
      continue

    with open(runtime_deps_file, "r") as f:
      runtime_deps[test_name] = [dep.strip() for dep in f]

  with open(deps_output_file, "w") as outfile:
    json.dump(
        runtime_deps, outfile, sort_keys=True, indent=2, separators=(",", ": "))


def CreateList(inputs, list_output_file):
  with open(list_output_file, "w") as f:
    f.write("\n".join(inputs))


def DoMain(argv):
  """Main method. Runs helper commands for generating unit test lists."""
  parser = optparse.OptionParser(
      """usage: %prog [<options>] <command> [<test names>]

      Valid commands:
          create_list       prints all given test names/args to a file, one line
                            per string
          pack_build        packs all test files from the given output directory
                            into a single test list file
          pack_run          packs all test and filter files from the given
                            output directory into a single test list file
      """)
  parser.add_option(
      "-o",
      action="store",
      dest="list_output_file",
      help="Output path in which to write the test list.")
  parser.add_option(
      "-d",
      action="store",
      dest="deps_output_file",
      help="Output path in which to write the runtime deps.")
  parser.add_option(
      "-t",
      action="store",
      dest="test_files_dir",
      help="Intermediate test list directory.")
  parser.add_option(
      "-a",
      action="store",
      dest="additional_runtime_options",
      help="Additional options applied to all tests.")
  options, inputs = parser.parse_args(argv)

  list_output_file = options.list_output_file
  deps_output_file = options.deps_output_file
  test_files_dir = options.test_files_dir
  additional_runtime_options = options.additional_runtime_options

  if len(inputs) < 1:
    parser.error("No command given.\n")
  command = inputs[0]
  test_names = inputs[1:]

  if not list_output_file:
    parser.error("Output path (-o) is required.\n")

  if command == "create_list":
    return CreateList(test_names, list_output_file)

  if command == "pack_build":
    if not test_files_dir:
      parser.error("pack_build require a test files directory (-t).\n")
    return CombineList(test_files_dir, list_output_file, False, None)

  if command == "pack_run":
    if not test_files_dir:
      parser.error("pack_run require a test files directory (-t).\n")
    if deps_output_file:
      CombineRuntimeDeps(test_files_dir, deps_output_file)
    return CombineList(test_files_dir, list_output_file, True,
                       additional_runtime_options)

  parser.error("Invalid command specified.")


if __name__ == "__main__":
  DoMain(sys.argv[1:])

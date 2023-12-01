#!/usr/bin/env python3
#
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool that combines a sequence of input proguard files and outputs a single
# proguard file.
#
# The final output file is formed by concatenating all of the
# input proguard files.


import argparse
import pathlib
import os
import sys
import json
REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build'))
import action_helpers  # pylint: disable=wrong-import-position

def _ReadFile(path):
  """Reads a file as a string."""
  return pathlib.Path(path).read_text()


def ReadProguardConfigsPath(build_config_path):
  """
  Reads the transitive proguard configs path of target whose build
  config path is `build_config_path`

  @param path: The path to the initial build_config
  @returns A set of the proguard config paths found during traversal.
  """
  proguard_configs_path = set()
  with open(build_config_path, 'r') as f:
    build_config = json.load(f)
    proguard_configs_path.update(build_config["deps_info"].get(
        "proguard_all_configs", set()))
    # java_library targets don't have `proguard_all_configs` so we need
    # to look at `proguard_configs` field instead.
    proguard_configs_path.update(build_config["deps_info"].get(
        "proguard_configs", set()))
  return proguard_configs_path

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output_file',
          help='Output file for the generated proguard file')
  parser.add_argument('--dep_file',
                      help='Depfile path to write the implicit inputs')

  args, input_files = parser.parse_known_args()
  # Fetch all proguard configs
  all_proguard_configs_path = set()
  for input_file in input_files:
    all_proguard_configs_path.update(ReadProguardConfigsPath(input_file))
  # Concatenate all proguard rules
  with open(args.output_file, 'w') as target:
    # Sort to maintain deterministic output.
    for proguard_config_path in sorted(all_proguard_configs_path):
      target.write(
          f"# -------- Config Path: {proguard_config_path.replace('../', '')} --------\n"
      )
      target.write(_ReadFile(proguard_config_path))
  action_helpers.write_depfile(
      args.dep_file, args.output_file, all_proguard_configs_path)



if __name__ == '__main__':
  sys.exit(main())

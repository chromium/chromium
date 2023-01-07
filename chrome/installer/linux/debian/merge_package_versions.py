#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import package_version_interval

if len(sys.argv) < 3:
  print ('Usage: %s output_deps_file input1_deps_file input2_deps_file ...' %
         sys.argv[0])
  sys.exit(1)

output_filename = sys.argv[1]
input_filenames = sys.argv[2:]

package_interval_sets = []
for input_filename in input_filenames:
  for line in open(input_filename):
    # Ignore blank lines
    if not line.strip():
      continue
    # Allow comments starting with '#'
    if line.startswith('#'):
      continue
    line = line.rstrip('\n')
    interval_set = package_version_interval.parse_interval_set(line)
    should_append_interval_set = True
    for i in range(len(package_interval_sets)):
      if package_interval_sets[i].implies(interval_set):
        should_append_interval_set = False
        break
      if interval_set.implies(package_interval_sets[i]):
        should_append_interval_set = False
        package_interval_sets[i] = interval_set
        break
    if should_append_interval_set:
      package_interval_sets.append(interval_set)

with open(output_filename, 'w') as output_file:
  lines = [interval_set.formatted() + '\n'
           for interval_set in package_interval_sets]
  output_file.write(''.join(sorted(lines)))

#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), os.path.pardir,
        os.path.pardir, os.path.pardir, os.path.pardir, 'build'))
import action_helpers

if len(sys.argv) < 3:
    print('Usage: %s output_deps_file input1_deps_file input2_deps_file ...' %
          sys.argv[0])
    sys.exit(1)

output_filename = sys.argv[1]
input_filenames = sys.argv[2:]

requires = set()
for input_filename in input_filenames:
    for line in open(input_filename):
        # Ignore blank lines
        if not line.strip():
            continue
        # Allow comments starting with '#'
        if line.startswith('#'):
            continue
        requires.add(line)

with action_helpers.atomic_output(output_filename, mode='w') as output_file:
    output_file.write(''.join(sorted(list(requires))))

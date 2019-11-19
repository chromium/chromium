#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Concatenates D-Bus busconfig files."""

import sys
import xml.etree.ElementTree


_BUSCONFIG_FILE_HEADER="""<!DOCTYPE busconfig
  PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
"""


def main():
  if len(sys.argv) < 3:
    sys.stderr.write('Usage: %s OUTFILE INFILES\n' % (sys.argv[0]))
    sys.exit(1)

  out_path = sys.argv[1]
  in_paths = sys.argv[2:]

  # Parse the first input file.
  tree = xml.etree.ElementTree.parse(in_paths[0])
  assert(tree.getroot().tag == 'busconfig')

  # Append the remaining input files to the first file.
  for path in in_paths[1:]:
    current_tree = xml.etree.ElementTree.parse(path)
    assert(current_tree.getroot().tag == 'busconfig')
    for child in current_tree.getroot():
      tree.getroot().append(child)

  # Output the result.
  with open(out_path, "w") as f:
    f.write(_BUSCONFIG_FILE_HEADER)
    tree.write(f)

if __name__ == '__main__':
  main()

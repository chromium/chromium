# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import sys
import os
import shutil


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--gen-dir',
                      help='Path to the generated directory',
                      required=True)
  parser.add_argument('--headers',
                      nargs='+',
                      help='Headers to copy over.',
                      required=True)
  args = parser.parse_args()
  for header in args.headers:
    # We have to copy-over some .cc files due to some C++ code doing #include "file.cc". See
    # crbug.com/421139881 for more information.
    if header.endswith(".h") or header.endswith(".cc"):
      # A soong output path looks like path/to/sandbox/gen/path/in/outputs. So everything
      # that comes after `gen/` is the actual output path that we need to copy to the new
      # destination.
      header_path = header.split("gen/")[1]
      shutil.copyfile(header, os.path.join(args.gen_dir, header_path))


if __name__ == '__main__':
  sys.exit(main())

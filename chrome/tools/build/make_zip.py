#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import sys
import zipfile


def main(args):
  if len(args) != 3:
    print 'usage: make_zip.py build_dir FILES.cfg output.zip'
    return 1
  (build_dir, cfg_file, output_file) = args

  exec_globals = {'__builtins__': None}
  execfile(cfg_file, exec_globals)

  cwd = os.getcwd()
  os.chdir(build_dir)

  files = set()
  for file_spec in exec_globals['FILES']:
    pattern = file_spec['filename']
    for glob_match in glob.glob(pattern):
      if os.path.isfile(glob_match):
        files.add(glob_match)
      elif os.path.isdir(glob_match):
        for root, dirs, filenames in os.walk(glob_match):
          for f in filenames:
            files.add(os.path.join(root, f));

  os.chdir(cwd)
  if not len(files):
    print 'error: no files found in %s' % build_dir
    return 1

  with zipfile.ZipFile(output_file, mode = 'w',
      compression = zipfile.ZIP_DEFLATED, allowZip64 = True) as output:
    for f in sorted(files):
      sys.stdout.write("%s\r" % f[:40].ljust(40, ' '))
      sys.stdout.flush()
      output.write(os.path.join(build_dir, f), f)

  print 'wrote %s' % output_file


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

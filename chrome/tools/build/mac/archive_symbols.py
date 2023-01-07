# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

# This script creates a BZ2-compressed TAR file for archiving Chrome symbols.

def Main(args):
  if len(args) < 2:
    print >> sys.stderr, "Usage: python archive_symbols.py file.tar.bz2 file..."
    return 1

  _RemoveIfExists(args[0])

  try:
    return subprocess.check_call(['tar', '-cjf'] + args)
  except:
    _RemoveIfExists(args[0])
    raise


def _RemoveIfExists(path):
  if os.path.exists(path):
    os.unlink(path)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

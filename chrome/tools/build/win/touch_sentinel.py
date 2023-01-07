# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

def main(args):
  """Touches a file.

  Args:
    args: An argument list, the first item of which is a file to touch.
  """
  try:
    os.makedirs(os.path.dirname(args[0]))
  except OSError:
    pass
  with open(args[0], 'a'):
    os.utime(args[0], None)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

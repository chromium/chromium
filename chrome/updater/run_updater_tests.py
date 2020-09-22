# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from test.integration_tests.common import path_finder
path_finder.add_typ_dir_to_sys_path()

import typ


def main():
  return typ.main(
      path=[path_finder.get_integration_tests_dir()],
  )


if __name__ == "__main__":
    sys.exit(main())

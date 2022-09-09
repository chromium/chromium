# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Paths for python proto modules and depot_tools.

This script must be used from inside a chromium checkout because it depends on
paths relative to the root of the checkout.
"""

import import_util
import os
import sys


# Expect this script to be located in <root_dir>/chrome/chrome_cleaner/tools.
_CURRENT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
_ROOT_DIRECTORY = os.path.normpath(os.path.join(_CURRENT_DIRECTORY, os.pardir,
                                                os.pardir, os.pardir))
assert(_CURRENT_DIRECTORY == os.path.join(_ROOT_DIRECTORY, 'chrome',
                                          'chrome_cleaner', 'tools'))


def GetBuildDirectory(build_configuration):
  """Returns the path to the build directory relative to this file.

  Args:
    build_configuration: name of the build configuration whose directory
        should be looked up, e.g. 'Debug' or 'Release'.
  """
  return os.path.join(_ROOT_DIRECTORY, 'out', build_configuration)


def GetChromiumRootDirectory():
  """Returns the path to root directory of the chromium checkout."""
  return _ROOT_DIRECTORY


def GetInternalRootDirectory():
  """Returns the path to //chrome/chrome_cleaner/internal."""
  return os.path.join(_ROOT_DIRECTORY, 'chrome', 'chrome_cleaner', 'internal')


def AddDepotToolsToPath():
  """Locates a depot_tools checkout and adds it to the Python import path.

  Returns:
    The path to depot_tools.
  """

  # Try to import find_depot_tools from the build subdir.
  import_util.AddImportPath(os.path.join(_ROOT_DIRECTORY, 'build'))
  import find_depot_tools
  return find_depot_tools.add_depot_tools_to_path()

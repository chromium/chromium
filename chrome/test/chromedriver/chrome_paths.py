# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Paths to common resources in the Chrome repository."""

import os


_THIS_DIR = os.path.abspath(os.path.dirname(__file__))


def GetSrc():
  """Returns the path to the root src directory."""
  return os.path.abspath(os.path.join(_THIS_DIR, os.pardir, os.pardir,
                                      os.pardir))


def GetTestData():
  """Returns the path to the src/chrome/test/data directory."""
  return os.path.join(GetSrc(), 'chrome', 'test', 'data')


def GetBuildDir(required_paths):
  """Returns the preferred build directory that contains given paths."""
  build_dir = os.path.join(GetSrc(), 'out', 'Default')
  for required_path in required_paths:
    if not os.path.exists(os.path.join(build_dir, required_path)):
      raise RuntimeError('Cannot find build directory containing ' +
                         ', '.join(required_paths))
  return build_dir

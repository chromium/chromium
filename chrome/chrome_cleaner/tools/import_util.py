# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for importing python scripts and proto modules."""

import os
import sys


def AddImportPath(path):
  """Adds the given path to the front of the Python import path."""
  sys.path.insert(0, os.path.normpath(path))


def AddProtosToPath(root_path):
  """Adds generated protobuf modules to the Python import path.

  Args:
    root_path: root directory where the pyproto subdir is located.
  """
  assert root_path is not None
  # Add the root pyproto dir so Python packages under that dir (such as
  # google.protobuf, which is built by //third_party/protobuf:py_proto) can be
  # imported with fully-qualified package names. (eg. "from google.protobuf
  # import text_format".)
  pyproto_dir = os.path.join(root_path, 'pyproto')
  AddImportPath(pyproto_dir)

  # Add individual directories with proto files that can be imported with bare
  # names. (eg. "import file_digest_pb2" to import file_digest.proto from the
  # chrome/chrome_cleaner/proto dir.)
  AddImportPath(os.path.join(pyproto_dir, 'chrome', 'chrome_cleaner', 'proto'))
  AddImportPath(os.path.join(pyproto_dir, 'chrome', 'chrome_cleaner', 'logging',
                             'proto'))

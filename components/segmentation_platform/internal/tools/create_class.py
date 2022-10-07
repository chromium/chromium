# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate header cc and unittest file for a class in chromium.

Usage:
  python3 components/segmentation_platform/internal/tools/create_class.py \
      src/dir/class_name.h

If any of these file already exists then prints a log and does not touch the
file, and creates the remaining files.

"""

import argparse
from datetime import date
import logging
import os
import sys

_HEADER_TEMPLATE = (
"""// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef {macro}
#define {macro}

namespace {namespace} {{

class {clas} {{
 public:
  {clas}();
  ~{clas}();

  {clas}({clas}&) = delete;
  {clas}& operator=({clas}&) = delete;

 private:
}};

}}

#endif  // {macro}
""")

_CC_TEMPLATE = (
"""// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

namespace {namespace} {{

{clas}::{clas} () = default;
{clas}::~{clas}() = default;

}}
""")

_TEST_TEMPLATE = (
"""// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

#include "testing/gtest/include/gtest/gtest.h"

namespace {namespace} {{

class {test_class} : public testing::Test {{
 public:
  {test_class}() = default;
  ~{test_class}() override = default;

  void SetUp() override {{
    Test::SetUp();
  }}

  void TearDown() override {{
    Test::TearDown();
  }}

 protected:
}};

TEST_F({test_class}, Test) {{
}}

}}
""")


def _GetLogger():
  """Logger for the tool."""
  logging.basicConfig(level=logging.INFO)
  logger = logging.getLogger('create_class')
  logger.setLevel(level=logging.INFO)
  return logger


def _WriteFile(path, type_str, contents):
  """Writes a file with contents to the path, if not exists."""
  if os.path.exists(path):
    _GetLogger().error('%s already exists', type_str)
    return

  _GetLogger().info('Writing %s file %s', type_str, path)
  with open(path, 'w') as f:
    f.write(contents)


def _GetClassNameFromFile(header):
  """Gets a class name from the header file name."""
  file_base = os.path.basename(header).replace('.h', '')
  class_name = ''
  for i in range(len(file_base)):
    if i == 0 or file_base[i - 1] == '_':
      class_name += file_base[i].upper()
    elif file_base[i] == '_':
      continue
    else:
      class_name += file_base[i]
  return class_name


def _CreateFilesForClass(args):
  """Creates header cc and test files for the class."""
  macro = args.header.replace('/', '_').replace('.', '_').upper() + '_'
  file_cc = args.header.replace('.h', '.cc')
  file_test = args.header.replace('.h', '_unittest.cc')
  class_name = _GetClassNameFromFile(args.header)

  contents = _HEADER_TEMPLATE.format(
      macro=macro,
      year=date.today().year,
      clas=class_name,
      namespace=args.namespace)
  _WriteFile(args.header, 'Header', contents)

  contents = _CC_TEMPLATE.format(
      year=date.today().year,
      clas=class_name,
      file_path=args.header,
      namespace=args.namespace)
  _WriteFile(file_cc, 'CC', contents)

  contents = _TEST_TEMPLATE.format(
      year=date.today().year,
      test_class=class_name + 'Test',
      file_path=args.header,
      namespace=args.namespace)
  _WriteFile(file_test, 'Test', contents)


def _CreateOptionParser():
  """Options parser for the tool."""
  parser = argparse.ArgumentParser(
      description='Create header, cc and test file for Chromium')
  parser.add_argument('header', help='Path to the header file from src/')
  parser.add_argument(
      '--namespace', dest='namespace', default='segmentation_platform')
  return parser


def main():
  parser = _CreateOptionParser()
  args = parser.parse_args()

  if '.h' not in args.header:
    raise ValueError('The first argument should be a path to header')

  _GetLogger().info('Creating class for header %s', args.header)
  _CreateFilesForClass(args)


if __name__ == '__main__':
  sys.exit(main())

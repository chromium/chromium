# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generates the list of valid imports for the lowest-supported version of
Windows.

Run from the root directory of the checkout - builds a .inc file that
will be included into delayloads_unittest.cc.

To export Chrome's view of the data for other build systems, run with the --json
flag and specify the --apisets-file.
"""

import argparse
import json
import os
import re
import sys

USE_PYTHON_3 = f'This script will only run under python3.'

# e.g. '  Section contains the following exports for CRYPT32.dll'
RE_NEWMOD = re.compile(
  'Section contains the following exports for (?P<dll>\w+\.(?i:dll|drv))')
# e.g. '       1020    0 00088A30 CertAddCRLContextToStore'
#                        ^ can be blank
RE_EXPORT = re.compile('^\s+\d+\s+[0-9A-F]+\s+[0-9A-F ]{8}\s+(?P<export>\w+)')
# apiset line in apisets.inc (see generate_supported_apisets.py)
RE_APISET = re.compile('^{"(?P<apiset>[^"]+)",\s*(?P<version>\d+)},')

def parse_file(f):
  """Naive parser for dumpbin output.

  f: filehandle to file containing dumpbin output."""
  mods = dict()
  curmod = None
  imports = []
  for line in f.readlines():
    # e.g. '  Section contains the following exports for CRYPT32.dll'
    m = re.search(RE_NEWMOD, line)
    if m:
      if curmod:
        mods[curmod] = imports
        imports = []
      curmod = m.group('dll').lower()
      continue
    if curmod is None:
      continue
    # e.g. '       1020    0 00088A30 CertAddCRLContextToStore'
    m = re.search(RE_EXPORT, line)
    if m:
      imports.append(m.group('export'))
  if curmod:
    mods[curmod] = imports
  return mods


def generate_inc(input_file):
  """Reads output of dumpbin /exports *.dll and makes input for .inc C++ file.

  input_file: path to file containing output of `dumpbin /exports *.dll`.

  using DetailedImports = std::map<std::string, std::set<std::string>>;
  """
  # const DetailedImports kVariable = {
  mods = parse_file(open(input_file, 'r', encoding='utf-8'))
  module_entries = [];
  for module, functions in mods.items():
    joined_functions = ',\n'.join([f'  "{fn}"' for fn in functions])
    module_line = f' {{"{module}", {{{joined_functions}}}}}'
    module_entries.append(module_line)
  all_modules = (',\n').join(module_entries)
  return all_modules
  # };


def maybe_read(filename):
  """ Read existing file so that we don't write it again if it hasn't changed"""
  if not os.path.isfile(filename):
    return None;
  try:
    with open(filename, 'r', encoding='utf-8') as f:
      return f.read();
  except Exception:
    return None


def write_imports_inc(input, output):
  existing_content = maybe_read(output)
  new_content = generate_inc(input)
  if existing_content == new_content:
    return
  os.makedirs(os.path.dirname(output), exist_ok=True)
  with open(output, 'w', encoding='utf-8', newline='') as f:
    f.write(new_content)


def parse_apisets(f):
  # Parses output of generate_supported_apisets.py
  apisets = dict()
  for line in f.readlines():
    m = re.search(RE_APISET, line)
    if m:
      apisets[m.group("apiset").lower()] = m.group("version")
  return apisets


def write_json(exports_file, apisets_file, output):
  # This generates a pbtext used in google3 for a similar check.
  exports = parse_file(open(exports_file, 'r', encoding='utf-8'))
  apisets = parse_apisets(open(apisets_file, 'r', encoding='utf-8'))

  result = {
    "exports": exports,
    "apisets": apisets,
  }
  with open(output, 'w', encoding='utf-8', newline='') as f:
      json.dump(result, f, indent=2)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('--exports-file',
                      default="chrome/test/delayload/supported_imports.txt",
                      metavar='FILE_NAME',
                      help='output of dumpbin /exports *.dll')
  parser.add_argument('--apisets-file',
                      default="chrome/test/delayload/apisets.inc",
                      metavar='FILE_NAME',
                      help='[optional] output of generate_supported_apisets.py')
  parser.add_argument('--out-file',
                      default='gen/chrome/test/delayload/supported_imports.inc',
                      metavar='FILE_NAME',
                      help='path to write .inc or .json file, within out-dir')
  parser.add_argument('--json', action='store_true',
                      help='output json instead of .inc')
  args, _extras = parser.parse_known_args()
  if args.json:
    # Used to export Chrome's data for other build systems.
    write_json(args.exports_file, args.apisets_file, args.out_file)
  else:
    # Used in Chrome build.
    write_imports_inc(args.exports_file, args.out_file)


if __name__ == '__main__':
  sys.exit(main())

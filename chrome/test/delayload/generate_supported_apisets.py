# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Parses .apiset section from a DLL (e.g. ApiSetSchema.dll) to determine which
functions are "exported" on a given Windows version by apisets. Only supports
version 6 (Windows 10+) of the schema.

This script is not run automatically as we do not ship a specimen ApiSetSchema
with the build.

vpython3 .\chrome\test\delayload\generate_supported_apisets.py \
     --dll c:\Windows\System32\ApisetSchema.dll \
     --out-file .\chrome\test\delayload\supported_apisets_10.0.10240.inc

See: https://www.geoffchappell.com/studies/windows/win32/apisetschema/index.htm
"""

import argparse
import hashlib
import os
import sys
import struct
import ctypes

USE_PYTHON_3 = f'This script will only run under python3.'

# Assume this script is under chrome\test\delayload
_SCRIPT_DIR = os.path.dirname(__file__)
_ROOT_DIR = os.path.join(_SCRIPT_DIR, os.pardir, os.pardir, os.pardir)
_PEFILE_DIR = os.path.join(_ROOT_DIR, 'third_party', 'pefile_py3')

sys.path.insert(1, _PEFILE_DIR)
import pefile


def from_utf16(data, start, length):
  return data[start:start + length].decode("utf-16-le")


class StructHelper(ctypes.Structure):
  def __init__(self, data):
    super(StructHelper, self).__init__()
    ctypes.memmove(ctypes.addressof(self), data, ctypes.sizeof(self))


class ApiSetNamespaceEntryV6(StructHelper):
  _fields_ = [
    ('Flags', ctypes.c_uint32),
    ('NameOffset', ctypes.c_uint32),
    ('NameLength', ctypes.c_uint32),
    ('HashedLength', ctypes.c_uint32),
    ('ValueOffset', ctypes.c_uint32),
    ('ValueCount', ctypes.c_uint32)
  ]


class ApiSetNamespaceV6(StructHelper):
  _fields_ = [
    ('Version', ctypes.c_uint32),
    ('Size', ctypes.c_uint32),
    ('Flags', ctypes.c_uint32),
    ('Count', ctypes.c_uint32),
    ('EntryOffset', ctypes.c_uint32),
    ('HashOffset', ctypes.c_uint32),
    ('HashFactor', ctypes.c_uint32)
  ]


class ApiSetSchemaV6:
  def __init__(self, data):
    self._data = data
    self._apisets = []
    header = ApiSetNamespaceV6(self._data)
    self._flag = header.Flags
    self._version = header.Version
    self._load_apisets(header.EntryOffset, header.Count)

  @property
  def apisets(self):
    return self._apisets

  def _load_apisets(self, offset, count):
    for _ in range(count):
      entry = ApiSetNamespaceEntryV6(
        self._data[offset:offset + ctypes.sizeof(ApiSetNamespaceEntryV6)])
      entry_name = from_utf16(self._data, entry.NameOffset, entry.NameLength)
      self._apisets.append(entry_name)
      offset += ctypes.sizeof(ApiSetNamespaceEntryV6)


def parse_apiset_names(data):
  version = struct.unpack("B", data[0:1])[0]
  if version != 6:
    raise Exception(f'Unsupported schema version: {version}')
  apiset_schema = ApiSetSchemaV6(data)
  return apiset_schema.apisets


def get_file_version(pe):
  for fileinfo in pe.FileInfo[0]:
    if fileinfo.Key.decode() == 'StringFileInfo':
      for st in fileinfo.StringTable:
        for entry in st.entries.items():
          if entry[0] == b'FileVersion':
            return entry[1].decode('utf8')
  raise Exception("FileVersion not found in dll")


def read_apiset_section(filename):
  pe = pefile.PE(filename)
  product_version = get_file_version(pe)
  for section in pe.sections:
    if section.Name == b'.apiset\0':
      return (product_version, section.get_data())
  raise Exception(".apiset section not Found")


# apiset_name: api-ms-win-core-synch-l1-2-0.dll
#  -> (api-ms-win-core-synch-l1-2, 0)
def apiset_dll_to_version(apiset_name):
   last_dash = apiset_name.rindex('-')
   apiset_maj_min = apiset_name[0:last_dash]
   apiset_subversion = apiset_name[last_dash+1:]
   return (apiset_maj_min, apiset_subversion)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('--dll',
                      metavar='FILE_NAME',
                      help='Dll with a .apiset section')
  parser.add_argument('--out-file',
                      default='apisets.inc',
                      metavar='FILE_NAME',
                      help='path to write .inc file to')
  args, _ = parser.parse_known_args()

  shasum = hashlib.sha256(open(args.dll, 'rb').read()).hexdigest()
  dll_basename = os.path.basename(args.dll)
  (dll_version, data) = read_apiset_section(args.dll)

  apiset_names = parse_apiset_names(data)
  # Only keep api- entries (skip ext- as these are for kernel modules).
  apiset_names = filter(lambda s: s.startswith("api-"), apiset_names)
  apiset_entries = [apiset_dll_to_version(s) for s in apiset_names]

  with open(args.out_file, 'w', encoding='utf8') as f:
     f.write(f'// Generated from {dll_basename}\n')
     f.write(f'// FileVersion: {dll_version}\n')
     f.write(f'// sha256: {shasum}\n')
     f.write(',\n'.join([f'{{"{e[0]}", {e[1]}}}' for e in apiset_entries]))
     f.write('\n')


if __name__ == '__main__':
  sys.exit(main())

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //components/reporting

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

from pathlib import Path
from pathlib import PurePath


# Header file names that do not need to be verified.
ALLOWED_HEADERS = {
    "errno.h",
    "snappy.h",
    "unistd.h",
}

ALLOWED_PREFIXES = {
    "build",
}

def CheckIncludeForFullPath(input_api, output_api):
  """Checks to make sure every .h file has a full path."""
  errors = []
  pattern = input_api.re.compile(r'^#include\s*[\"<](\S*)[\">]')
  for filename in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not filename.LocalPath().endswith((".h", ".cc")):
      continue
    contents = input_api.ReadFile(filename)
    for lineno, line in enumerate(contents.splitlines(), start=1):
      m = pattern.match(line)
      if not m:
        continue

      header = PurePath(m.group(1))
      if header.suffix != ".h":
        continue
      if header.name in ALLOWED_HEADERS:
        continue
      if header.parts[0] in ALLOWED_PREFIXES:
        continue
      if not header.is_absolute():
        header = '../../' / header
      if tuple(header.suffixes) == (".pb", ".h"):
        # *.pb.h files don't exist in source code, replace the suffix with
        # *.proto
        header = header.with_suffix("").with_suffix(".proto")
      if not Path(header).is_file():
        errors.append(
            filename.LocalPath() + ":" + str(lineno) + " " + m.group(1))

  if errors:
    return [ output_api.PresubmitError(
        'Some #include(s) in source errors do not have a full path. ',
        errors) ]
  return []

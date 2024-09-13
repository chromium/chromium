# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os.path
from typing import List, Callable
import re

import constants
from metadata import Metadata
from pathlib import Path
from mapper import MapperException
from license_type import LicenseType

# The mandatory metadata fields for a single dependency.
KNOWN_FIELDS = {
    "Name",  # Short name (for header on about:credits).
    "URL",  # Project home page.
    "License",  # Software license.
    "License File",  # Relative paths to license texts.
    "Shipped",  # Whether the package is in the shipped product.
    "Version",  # The version for the package.
    "Revision",  # This is equivalent to Version but Chromium is lenient.
}

# The metadata fields that can have multiple values.
MULTIVALUE_FIELDS = {
    "License",
    "License File",
}
# Line used to separate dependencies within the same metadata file.
PATTERN_DEPENDENCY_DIVIDER = re.compile(r"^-{20} DEPENDENCY DIVIDER -{20}$")

# The delimiter used to separate multiple values for one metadata field.
VALUE_DELIMITER = ","

_RAW_LICENSE_TO_FORMATTED_DETAILS = {
    "BSD": ("BSD", LicenseType.NOTICE, "SPDX-license-identifier-BSD"),
    "BSD 3-Clause": (
        "BSD_3_CLAUSE", LicenseType.NOTICE,
        "SPDX-license-identifier-BSD-3-Clause"),
    "Apache 2.0": (
        "APACHE_2_0", LicenseType.NOTICE, "SPDX-license-identifier-Apache-2.0"),
    "MIT": ("MIT", LicenseType.NOTICE, "SPDX-license-identifier-MIT"),
    "Unicode": (
        "UNICODE", LicenseType.NOTICE,
        "SPDX-license-identifier-Unicode-DFS-2016"),
    "MPL 1.1":
      ("MPL", LicenseType.RECIPROCAL, "SPDX-license-identifier-MPL-1.1"),
    "unencumbered":
      ("UNENCUMBERED", LicenseType.UNENCUMBERED,
       "SPDX-license-identifier-Unlicense"),
}


def get_license_type(license: str) -> LicenseType:
  """Return the equivalent license type for the provided string license."""
  if license in _RAW_LICENSE_TO_FORMATTED_DETAILS:
    return _RAW_LICENSE_TO_FORMATTED_DETAILS[license][1]
  raise None


def get_license_bp_name(license: str) -> str:
  return _RAW_LICENSE_TO_FORMATTED_DETAILS[license][2]


def is_ignored_readme_chromium(path: str) -> bool:
  return path in constants.IGNORED_README


def get_most_restrictive_type(licenses: List[str]) -> LicenseType:
  """Returns the most restrictive license according to the values of LicenseType."""
  most_restrictive = LicenseType.UNKNOWN
  for license in licenses:
    if _RAW_LICENSE_TO_FORMATTED_DETAILS[license][
      1].value > most_restrictive.value:
      most_restrictive = _RAW_LICENSE_TO_FORMATTED_DETAILS[license][1]
  return most_restrictive


def get_license_file_format(license: str):
  """Return a different representation of the license that is better suited
  for file names."""
  if license in _RAW_LICENSE_TO_FORMATTED_DETAILS:
    return _RAW_LICENSE_TO_FORMATTED_DETAILS[license][0]
  raise None


class InvalidMetadata(Exception):
  """This exception is raised when metadata is invalid."""
  pass


def parse_chromium_readme_file(readme_path: str,
    post_process_operation: Callable = None) -> Metadata:
  """Parses the metadata from the file.

  Args:
    readme_path: the path to a file from which to parse metadata.
    post_process_operation: Operation done on the dictionary after parsing
    metadata, this callable must return a dictionary.

  Returns: the metadata for all dependencies described in the file.

  Raises:
    InvalidMetadata - Raised when the metadata can't be parsed correctly. This
    could happen due to plenty of reasons (eg: unidentifiable license, license
    file path does not exist or duplicate fields).
  """
  field_lookup = {name.lower(): name for name in KNOWN_FIELDS}

  dependencies = []
  metadata = {}
  for line in Path(readme_path).read_text().split("\n"):
    line = line.strip()
    # Skip empty lines.
    if not line:
      continue

    # Check if a new dependency will be described.
    if re.match(PATTERN_DEPENDENCY_DIVIDER, line):
      # Save the metadata for the previous dependency.
      if metadata:
        dependencies.append(metadata)
      metadata = {}
      continue

    # Otherwise, try to parse the field name and field value.
    parts = line.split(": ", 1)
    if len(parts) == 2:
      raw_field, value = parts
      field = field_lookup.get(raw_field.lower())
      if field:
        if field in metadata:
          # Duplicate field for this dependency.
          raise InvalidMetadata(f"duplicate '{field}' in {readme_path}")
        if field in MULTIVALUE_FIELDS:
          metadata[field] = [
              entry.strip() for entry in value.split(VALUE_DELIMITER)
          ]
        else:
          metadata[field] = value

    # The end of the file has been reached. Save the metadata for the
    # last dependency, if available.
  if metadata:
    dependencies.append(metadata)

  if len(dependencies) == 0:
    raise Exception(
        f"Failed to parse any valid metadata from \"{readme_path}\"")

  try:
    if post_process_operation is None:
      post_process_operation = constants.POST_PROCESS_OPERATION.get(readme_path,
                                                                    lambda
                                                                        _metadata: _metadata)
    metadata = Metadata(post_process_operation(dependencies[0]))
  except MapperException:
    raise Exception(f"Failed to post-process f{readme_path}")

  for license in metadata.get_licenses():
    if not license in _RAW_LICENSE_TO_FORMATTED_DETAILS:
      raise InvalidMetadata(
          f"\"{readme_path}\" contains unidentified license \"{license}\"")
  if not metadata.get_license_file_path():
    raise InvalidMetadata(f"License file path not declared in {readme_path}")
  return metadata


def resolve_license_path(repo_path: str, license_path: str) -> str:
  if license_path.startswith("//"):
    # This is an absolute path that starts from the root of external/cronet
    # repository, we should not use the directory path for resolution here.
    # See https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/bytes/v1/README.chromium as
    # an example of such case.
    return os.path.abspath(os.path.relpath(license_path[2:], repo_path))
  # Relative path from the README.chromium, return as-is.
  return license_path

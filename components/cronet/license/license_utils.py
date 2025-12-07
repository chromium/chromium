# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
    "CPEPrefix",  # NIST Common Platform Enumeration identifier; used for security tagging.
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


def get_license_type(license_name: str) -> LicenseType:
    """Return the equivalent license type for the provided string license."""
    return constants.RAW_LICENSE_TO_FORMATTED_DETAILS[license_name][1]


def get_license_bp_name(license_name: str) -> str:
    return constants.RAW_LICENSE_TO_FORMATTED_DETAILS[license_name][2]


def is_ignored_readme_chromium(path: str) -> bool:
    return path in constants.IGNORED_README


def get_license_file_format(license_name: str):
    """Return a different representation of the license that is better suited
  for file names."""
    return constants.RAW_LICENSE_TO_FORMATTED_DETAILS[license_name][0]


class InvalidMetadata(Exception):
    """This exception is raised when metadata is invalid."""


def parse_chromium_readme_file(readme_path: str,
                               post_process_operation: Callable = None
                               ) -> Metadata:
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
                    raise InvalidMetadata(
                        f"duplicate '{field}' in {readme_path}")
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
            post_process_operation = constants.POST_PROCESS_OPERATION.get(
                readme_path, lambda _metadata: _metadata)
        metadata = Metadata(post_process_operation(dependencies[0]))
    except MapperException as e:
        raise Exception(f"Failed to post-process {readme_path}") from e

    for license_name in metadata.get_licenses():
        if not license_name in constants.RAW_LICENSE_TO_FORMATTED_DETAILS:
            raise InvalidMetadata(
                f"\"{readme_path}\" contains unidentified license \"{license_name}\""
            )
    return metadata


def resolve_license_path(readme_chromium_path: str, license_path: str) -> str:
    """
  Resolves the relative path from the repository root to the license file.

  :param readme_chromium_path: Relative path to the README.chromium starting
  from the root of the repository.
  :param license_path: The field value of `License File` in the README.chromium.
  If the value of the license_path starts with `//` then that means that the
  license file path is already relative from the repo path. Otherwise, it is
  assumed that the provided path is relative from the README.chromium path.
  :return: The relative path from the repository root to the declared license
  file.
  """
    if license_path.startswith("//"):
        # This is an relative path that starts from the root of external/cronet
        # repository, we should not use the directory path for resolution here.
        # See https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/bytes/v1/README.chromium as
        # an example of such case.
        return license_path[2:]
    # Relative path from the README.chromium, append the path from root of repo
    # until the README.chromium so it becomes a relative path from the root of
    # repo.
    return os.path.join(readme_chromium_path, license_path)

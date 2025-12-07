# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Dict, List, Union
import metadata_dictionary
import constants
from license_type import LicenseType


def get_most_restrictive_type(licenses_names: List[str]) -> LicenseType:
    """Returns the most restrictive license according to the values of LicenseType."""
    most_restrictive = LicenseType.UNKNOWN
    for license_name in licenses_names:
        if constants.RAW_LICENSE_TO_FORMATTED_DETAILS[license_name][
                1].value > most_restrictive.value:
            most_restrictive = constants.RAW_LICENSE_TO_FORMATTED_DETAILS[
                license_name][1]
    return most_restrictive


class Metadata:

    def __init__(self, metadata_dict: Dict[str, Union[str, List[str]]]):
        self.metadata = metadata_dict

    def get_name(self) -> str:
        return self.metadata["Name"]

    def get_url(self) -> str:
        return self.metadata["URL"]

    def get_version(self):
        if not self._get_version() or self._get_version() in [
                "0", "unknown", "N/A"
        ]:
            # This is a heuristic try to avoid putting a version when the version
            # in the README.chromium does not make any sense.
            return self._get_revision()
        return self._get_version()

    def _get_version_control(self):
        """Returns the VCS of the URL provided if possible,
    otherwise None is returned."""
        if "git" in self.get_url() or "googlesource" in self.get_url():
            return "Git"
        if "hg" in self.get_url():
            return "Hg"
        return None

    def _create_identifier_block(
            self) -> metadata_dictionary.MetadataDictionary:
        identifier_dictionary = metadata_dictionary.MetadataDictionary(
            "identifier")
        identifier_dictionary["value"] = f"\"{self.get_url()}\""
        identifier_dictionary["type"] = f"\"{self._get_version_control()}\""
        if self.get_version():
            identifier_dictionary["version"] = f"\"{self.get_version()}\""
        return identifier_dictionary

    def _get_version(self) -> str:
        return self.metadata.get("Version", None)

    def _get_revision(self) -> str:
        return self.metadata.get("Revision", None)

    def get_licenses(self) -> List[str]:
        return self.metadata["License"]

    def get_license_file_path(self) -> str:
        return self.metadata.get("License File", [None])[0]

    def get_license_type(self) -> LicenseType:
        return get_most_restrictive_type(self.get_licenses())

    def to_android_metadata(self):
        third_party_dict = metadata_dictionary.MetadataDictionary(
            "third_party")
        third_party_dict["license_type"] = self.get_license_type().name
        if self.get_version():
            third_party_dict["version"] = f"\"{self.get_version()}\""

        if self._get_version_control():
            third_party_dict[
                "identifier_primary"] = self._create_identifier_block()
        else:
            third_party_dict["homepage"] = f"\"{self.get_url()}\""

        cpe_prefix = self.metadata.get("CPEPrefix")
        if cpe_prefix is not None:
            third_party_dict["security"] = security_dict = metadata_dictionary.MetadataDictionary("security")
            security_dict["tag"] = f"\"NVD-CPE2.3:{cpe_prefix}\""

        return "\n".join(
            [f"name: \"{self.get_name()}\"", f"{third_party_dict}"])

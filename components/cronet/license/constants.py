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

from typing import Callable, Dict, List, Union
from mapper import Mapper


def create_license_post_processing(*args: Mapper) -> Callable:
  def __update_metadata(metadata: Dict[str, Union[str, List[str]]]) -> Dict[
      str, Union[str, List[str]]]:
    for mapper in args:
      mapper.write(metadata)
    return metadata

  return __update_metadata


# This is relative to the repo_directory passed in |update_license|
# post-processing is necessary for the cases where the license is not in the
# standard format, this can include two or more license (eg: "Apache 2 and MPL")
# or a single license that is not easily identifiable (eg: "BSDish")
#
# The current structure is Mapper(dictionary_key, expected_value, value_to_write)
POST_PROCESS_OPERATION = {
    "base/third_party/nspr/README.chromium": create_license_post_processing(
        Mapper("License", ['MPL 1.1/GPL 2.0/LGPL 2.1'], ["MPL 1.1"])),
    "url/third_party/mozilla/README.chromium": create_license_post_processing(
        Mapper("License", ['BSD and MPL 1.1/GPL 2.0/LGPL 2.1'],
               ["BSD"])),
    "third_party/libc++abi/README.chromium": create_license_post_processing(
        Mapper("License",
               ['MIT',
                'University of Illinois/NCSA Open Source License'],
               ["MIT"])),
    "third_party/libc++/README.chromium": create_license_post_processing(
        Mapper("License",
               ['MIT',
                'University of Illinois/NCSA Open Source License'],
               ["MIT"])),
    "third_party/boringssl/README.chromium": create_license_post_processing(
        Mapper("License", ['BSDish'], ["BSD"]),
        # TODO(b/360316861): Fix upstream by setting an explicit version to boringssl.
        Mapper("Version", "git", None)),
    "net/third_party/quiche/METADATA": create_license_post_processing(
        # TODO(b/360316861): Fix upstream by setting an explicit version to QUICHE.
        Mapper("Version", "git", None)),
    # TODO(b/360316861): Fix this upstream in Chromium.
    "third_party/quic_trace/README.chromium": create_license_post_processing(
        Mapper("Version", "git", "caa0a6eaba816ecb737f9a70782b7c80b8ac8dbc")),
    "third_party/metrics_proto/README.chromium": create_license_post_processing(
        Mapper("URL", "This is the canonical public repository", "Piper")),
    "third_party/boringssl/src/pki/testdata/nist-pkits/README.chromium": create_license_post_processing(
        Mapper("License", [
            'Public Domain: United States Government Work under 17 U.S.C. 105'],
               ["unencumbered"]),
        Mapper("License File", "", "N/A")),
}

# This is relative to the repo_directory passed in |update_license|
IGNORED_README = {
    # Not a third-party.
    "testing/android/native_test/README.chromium",
    # Not a third-party.
    "build/internal/README.chromium",
}

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Callable, Dict, List, Union
from mapper import Mapper
from license_type import LicenseType

def create_license_post_processing(*args: Mapper) -> Callable:
  def __update_metadata(metadata: Dict[str, Union[str, List[str]]]) -> Dict[
    str, Union[str, List[str]]]:
    for mapper in args:
      mapper.write(metadata)
    return metadata

  return __update_metadata

RAW_LICENSE_TO_FORMATTED_DETAILS = {
    "blessing": ("blessing", LicenseType.UNENCUMBERED, "SPDX-license-identifier-blessing"),
    "BSD": ("BSD", LicenseType.NOTICE, "SPDX-license-identifier-BSD"),
    "BSD-2-Clause": ("BSD_2_CLAUSE", LicenseType.NOTICE, "SPDX-license-identifier-BSD-2-Clause"),
    "BSD 3-Clause": (
        "BSD_3_CLAUSE", LicenseType.NOTICE,
        "SPDX-license-identifier-BSD-3-Clause"),
    "BSD-3-Clause": (
        "BSD_3_CLAUSE", LicenseType.NOTICE,
        "SPDX-license-identifier-BSD-3-Clause"),
    "Apache 2.0": (
        "APACHE_2_0", LicenseType.NOTICE, "SPDX-license-identifier-Apache-2.0"),
    # Different Apache 2.0 format used in Chromium.
    "Apache-2.0": (
        "APACHE_2_0", LicenseType.NOTICE, "SPDX-license-identifier-Apache-2.0"),
    "MIT": ("MIT", LicenseType.NOTICE, "SPDX-license-identifier-MIT"),
    "Unicode-3.0": (
        "UNICODE_3_0", LicenseType.NOTICE,
        "SPDX-license-identifier-Unicode-3.0"),
    "Unicode-DFS-2016": (
        "UNICODE", LicenseType.NOTICE,
        "SPDX-license-identifier-Unicode-DFS-2016"),
    "ICU": (
        "ICU", LicenseType.NOTICE,
        "SPDX-license-identifier-ICU"),
    "Zlib":
      ("ZLIB", LicenseType.RECIPROCAL, "SPDX-license-identifier-Zlib"),
    "MPL 1.1":
      ("MPL", LicenseType.RECIPROCAL, "SPDX-license-identifier-MPL-1.1"),
    "MPL-1.1":
      ("MPL", LicenseType.RECIPROCAL, "SPDX-license-identifier-MPL-1.1"),
    "MPL 2.0":
      ("MPL", LicenseType.RECIPROCAL, "SPDX-license-identifier-MPL-2.0"),
    "NCSA":
      ("NCSA", LicenseType.NOTICE, "SPDX-license-identifier-NCSA"),
    "unencumbered":
      ("UNENCUMBERED", LicenseType.UNENCUMBERED,
       "SPDX-license-identifier-Unlicense"),
}

# This is relative to the repo_directory passed in |update_license|
# post-processing is necessary for the cases where the license is not in the
# standard format, this can include two or more license (eg: "Apache 2 and MPL")
# or a single license that is not easily identifiable (eg: "BSDish")
#
# The current structure is Mapper(dictionary_key, expected_value, value_to_write)
POST_PROCESS_OPERATION = {
    "url/third_party/mozilla/README.chromium": create_license_post_processing(
        Mapper("License", ['MPLv2'], ["MPL 2.0"])),
    "third_party/apache-portable-runtime/README.chromium": create_license_post_processing(
        Mapper("License", ['Apache-2.0', 'dso', 'Zlib', 'ISC', 'BSD-4-Clause-UC'], ["Apache 2.0"])),
    "third_party/compiler-rt/README.chromium": create_license_post_processing(
        Mapper("License",
               ['NCSA', 'Apache-with-LLVM-Exception', 'MIT'],
               ["MIT"])),
    "third_party/libc++abi/README.chromium": create_license_post_processing(
        Mapper("License",
               ['NCSA', 'Apache-with-LLVM-Exception', 'MIT'],
               ["MIT"])),
    "third_party/libc++/README.chromium": create_license_post_processing(
        Mapper("License",
               ['NCSA', 'Apache-with-LLVM-Exception', 'MIT'],
               ["MIT"])),
    "third_party/boringssl/README.chromium": create_license_post_processing(
        Mapper("License", ['MIT', 'BSD-3-Clause', 'OpenSSL', 'ISC', 'SSLeay'], ["BSD"]),
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
               ["unencumbered"])),
}

# This is relative to the repo_directory passed in |update_license|
IGNORED_README = {
    # Not a third-party.
    "testing/android/native_test/README.chromium",
    # Not a third-party.
    "build/internal/README.chromium",
    # The real README.chromium lives nested inside each dependency.
    "third_party/android_deps/README.chromium",
    # This is not used in AOSP and not imported.
    "third_party/junit/README.chromium",
    # The real README.chromium lives nested inside each dependency.
    "third_party/androidx/README.chromium",
    # This is not used in AOSP and not imported.
    "third_party/aosp_dalvik/README.chromium",
    # b/369075726, those crates are missing LICENSE files upstream, once fixed
    # and imported, we will create a README for those.
    "third_party/rust/rstest/v0_17/README.chromium",
    "third_party/rust/rustc_demangle_capi/v0_1/README.chromium",
    "third_party/rust/rstest_macros/v0_17/README.chromium",
    "third_party/rust/codespan_reporting/v0_11/README.chromium",
    "third_party/rust/rstest_reuse/v0_5/README.chromium",
}

# READMEs that should have been discovered through gn, but were not, e.g.
# because they don't have a corresponding BUILD.gn file.
# TODO: http://crbug.com/389925432 - remove the need for this list.
INCLUDED_README = {
  "base/third_party/nspr/README.chromium",
  "url/third_party/mozilla/README.chromium",
}

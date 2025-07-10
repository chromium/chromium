# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a header file defining expected properties of the ICU data file."""

import argparse
import hashlib
import os

def gen_sha256(input_file_path):
    """
    Returns the upper-case hex-encoded SHA256 hash of an input file.
    """
    with open(input_file_path, "rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest().upper()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--icu_file", help="Path to the ICU data file.")
    parser.add_argument("--output", help="Path to the output header file.")
    args = parser.parse_args()

    size = os.path.getsize(args.icu_file)
    checksum = gen_sha256(args.icu_file)

    with open(args.output, "w") as f:
        f.write(rf"""// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>

namespace enterprise_companion {{

// The size (in bytes) of the expected ICU data file.
inline constexpr int64_t kExpectedIcuFileSize = {size};

// The hex-encoded SHA256 checksum of the expected ICU data file.
inline constexpr std::string_view kExpectedIcuFileChecksum =
    "{checksum}";

}}  // namespace enterprise_companion
""")




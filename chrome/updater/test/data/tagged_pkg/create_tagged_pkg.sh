#!/bin/bash

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
set -eu

# Script to create a tagged PKG file for testing.
# Usage: ./create_tagged_pkg.sh <output_path> <brand_code>

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <output_path> <brand_code>"
  exit 1
fi

OUTPUT_PATH=$1
BRAND=$2

TEMP_DIR=$(mktemp -d /tmp/test_pkg_dir.XXXXXX)
TEMP_PKG="${TEMP_DIR}/test.pkg"

# Create dummy PKG.
pkgbuild --identifier com.google.Chrome.Test --version 1.0.0.0 \
    --nopayload "${TEMP_PKG}"

# Inject tag.
# We assume tag_exe is built and available in out/Default
# If not, this script will fail.
TAG_EXE="out/Default/tag_exe"
if [[ ! -x "${TAG_EXE}" ]]; then
  echo "Error: ${TAG_EXE} not found or not executable. Build it first!"
  rm -rf "${TEMP_DIR}"
  exit 1
fi

"${TAG_EXE}" --set-tag="brand=${BRAND}" --out="${OUTPUT_PATH}" "${TEMP_PKG}"

rm -rf "${TEMP_DIR}"
echo "Created tagged PKG at ${OUTPUT_PATH}"

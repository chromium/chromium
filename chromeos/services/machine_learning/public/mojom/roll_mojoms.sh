#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

usage() {
  echo "Usage: $0 CROS_SRC_DIR"
  echo ""
  echo "Rolls (copies) ML Service *.mojom files from Chrome OS to current"
  echo "directory, with appropriate boilerplate modifications for use in"
  echo "Chromium."
  echo ""
  echo "CROS_SRC_DIR: Path to Chromium OS source, e.g. ~/chromiumos/src."
}

CROS_SRC_DIR="$1"

if [ -z "$CROS_SRC_DIR" ]; then
  usage
  exit 1
fi

if [ ! -d "$CROS_SRC_DIR" ]; then
  echo "$CROS_SRC_DIR not a directory"
  usage
  exit 1
fi

if [ "$(basename $CROS_SRC_DIR)" != "src" ]; then
  echo "$CROS_SRC_DIR should end with /src"
  usage
  exit 1
fi

if ! git diff-index --quiet HEAD -- ; then
  echo "'git status' not clean. Commit your changes first."
  exit 1
fi

readonly EXPECTED_PATH="chromeos/services/machine_learning/public/mojom"
if [[ "$(pwd)" != *"${EXPECTED_PATH}" ]]; then
  echo "Please run from within ${EXPECTED_PATH}."
  exit 1;
fi

echo "Copying mojoms from Chrome OS side ..."
cp $1/platform2/ml/mojom/*.mojom . || exit 1

echo "Removing big_buffer.mojom ..."
rm big_buffer.mojom || exit 1

echo "Removing file_path.mojom ..."
rm file_path.mojom || exit 1

echo "Removing geometry.mojom ..."
rm geometry.mojom || exit 1

echo "Removing shared_memory.mojom ..."
rm shared_memory.mojom || exit 1

echo "Removing time.mojom ..."
rm time.mojom || exit 1

echo "Removing web_platform_model.mojom ..."
rm web_platform_model.mojom || exit 1

echo "Changing import paths ..."
sed --in-place --regexp-extended \
  -e 's~^import "ml/mojom/big_buffer.mojom~import "mojo/public/mojom/base/big_buffer.mojom~g' \
  -e 's~^import "ml/mojom/file_path.mojom~import "mojo/public/mojom/base/file_path.mojom~g' \
  -e 's~^import "ml/mojom/geometry.mojom~import "ui/gfx/geometry/mojom/geometry.mojom~g' \
  -e 's~^import "ml/mojom/shared_memory.mojom~import "mojo/public/mojom/base/shared_memory.mojom~g' \
  -e 's~^import "ml/mojom/time.mojom~import "mojo/public/mojom/base/time.mojom~g' \
  -e 's~^import "ml~import "chromeos/services/machine_learning/public~g' \
  *.mojom

echo "OK. Now:"
echo "1. Examine 'git diff' to double-check the results of this tool."
echo "2. After submitting, also update any google3 files generated from these "
echo "   mojoms (e.g. javascript bindings)."

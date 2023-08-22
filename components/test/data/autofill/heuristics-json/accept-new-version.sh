#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copies the ".json.new" files over the ".json" files to release a new version
# of ground truth.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

for FILE in "${SCRIPT_DIR}"/**/*.json; do
  if [[ -f "${FILE}.new" ]]; then
    mv "${FILE}.new" "${FILE}"
  fi
done

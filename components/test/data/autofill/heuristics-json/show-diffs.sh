#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shows a diff between ".json" and ".json.new" files.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

shopt -s globstar  # Enable **

for FILE in "${SCRIPT_DIR}"/**/*.json; do
  if [[ -f "${FILE}.new" ]]; then
    echo "${FILE} has different results:"
    diff -U6 "${FILE}" "${FILE}.new"
  else
    echo "${FILE} was not changed"
  fi
done

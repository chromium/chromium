#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o errexit  # Stop the script on the first error.
set -o nounset  # Catch un-initialized variables.

SHEET_URL="https://docs.google.com/spreadsheets/u/1/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/export"
UNPROCESSED_REQUIRED_COVERAGE_TESTS_TAB=2008870403
ACTIONS_TAB=1864725389

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

curl -L \
  "${SHEET_URL}?gid=${UNPROCESSED_REQUIRED_COVERAGE_TESTS_TAB}&format=csv" \
  > "${DIR}/data/coverage_required.csv"
curl -L "${SHEET_URL}?gid=${ACTIONS_TAB}&format=csv" \
  > "${DIR}/data/actions.csv"

echo "Done!"

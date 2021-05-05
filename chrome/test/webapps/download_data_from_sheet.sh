#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o errexit  # Stop the script on the first error.
set -o nounset  # Catch un-initialized variables.

SHEET_URL="https://docs.google.com/spreadsheets/u/1/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/export"
COVERAGE_TESTS_GID=2008870403
ACTIONS_GID=1864725389
PARTIAL_COVERAGE_GID=452077264
AUTOMATED_TESTS_GID=1894585254
MANUAL_TESTS_GID=1424278080

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

curl -L "${SHEET_URL}?gid=${COVERAGE_TESTS_GID}&format=csv" \
  > "${DIR}/data/coverage_required.csv"
curl -L "${SHEET_URL}?gid=${ACTIONS_GID}&format=csv" \
  > "${DIR}/data/actions.csv"
curl -L "${SHEET_URL}?gid=${PARTIAL_COVERAGE_GID}&format=csv" \
  > "${DIR}/data/partial_coverage_paths.csv"
curl -L "${SHEET_URL}?gid=${AUTOMATED_TESTS_GID}&format=csv" \
  > "${DIR}/data/automated_tests.csv"
curl -L "${SHEET_URL}?gid=${MANUAL_TESTS_GID}&format=csv" \
  > "${DIR}/data/manual_tests.csv"

# Note: This script does NOT populate
# //chrome/test/web_apps/data/framework_actions_*.csv. This is intentional.
# These files should be updated manually by the person adding actions to the
# integration test framework.

echo "Done!"

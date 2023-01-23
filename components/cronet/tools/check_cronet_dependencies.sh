#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# check_cronet_dependencies.sh - Prints new Cronet's deps to stdout.
#
# Arguments:
#   $1: relative path to file containing old/known Cronet's dependencies.
#
# Output:
#   Exit code 0: deduped list of new Cronet's dependencies to stdout.
#   Otherwise: something went wrong.

set -e

OLD_DEPS=$(realpath "$1")
GN_PATH=$(realpath "../../buildtools/linux64/gn")
TMP_DIR=$(mktemp -d -p ./)
trap "rm -rf ${TMP_DIR}" EXIT

# gn desc *might* modify the target build dir. To be sure that doesn't affect
# the current build, create a new build dir with the same gn args and run gn
# desc there instead.
GN_ARGS=$(cat args.gn | tr '\n' ' ' | sed "s/ = /=/g")
"$GN_PATH" gen "$TMP_DIR" --args="$GN_ARGS" &>/dev/null

(
cd $TMP_DIR
NEW_DEPS=$(mktemp -p ./)

# Create new dependencies list and drop duplicates.
"$GN_PATH" desc ./ //components/cronet/android:cronet deps --all |
  awk -F ':' '{new_deps[$1]=1}; END{for(dep in new_deps) print dep}' |
  sort > "$NEW_DEPS"

# Filter out dependencies that already existed. I.e., print new dependencies to
# stdout.
awk 'NR==FNR {old_deps[$0]=1; next} {if(!old_deps[$0]) print $0}' \
  "$OLD_DEPS" "$NEW_DEPS" |
  sort
)

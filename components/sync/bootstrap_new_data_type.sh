#!/bin/bash

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script used to bootstrap client-side integration with components/sync. Given
# a data type name in upper underscore case (e.g. FOO_BAR) and a bug number,
# it will generate a new branch with a single commit adding:
#   - A new foo_bar_specifics.proto file with an empty FooBarSpecifics message,
#     hooked to entity_specifics.proto.
#   - A new DataType::FOO_BAR.
#   - TODOs for next CLs, and the order in which they should be followed.
# In case of failure the user should be left in the original branch.
#
# Requirements: awk, tr

# Branch created by the script.
BRANCH_NAME=new-data-type

# Converts "FOO_BAR" to "foo_bar".
to_lower_snake_case() {
  echo "$1" | tr '[:upper:]' '[:lower:]'
}

# Converts "FOO_BAR" to "FooBar".
to_camel_case() {
  echo "$1" | awk -F'_' '{for (i=1; i<=NF; i++) { $i=tolower($i); $i=toupper(substr($i,1,1)) tolower(substr($i,2)) } print $0}' OFS=''
}

# Converts "FOO_BAR" to "Foo Bar".
to_title_case() {
  echo "$1" | awk -F'_' '{for (i=1; i<=NF; i++) { $i=tolower($i); $i=toupper(substr($i,1,1)) tolower(substr($i,2)) } print $0}' OFS=' '
}

# Usage: replace_string_in_file <file> <old_str> <new_str>
# Replaces all instances.
replace_string_in_file() {
  local file="$1"
  local old_str="$2"
  local new_str="$3"
  awk "{gsub(/$old_str/, \"$new_str\"); print}" "$file" > tmp && mv tmp "$file"
}

if [[ "$#" -ne 2 ]]; then
  echo "Usage: $0 <DataType, e.g. FOO_BAR> <BugNumber, e.g. 123>"
  exit 1
fi

if [[ ! "$1" =~ ^[A-Z]+(_[A-Z]+)*$ ]]; then
  echo "Value for DataType ($1) is not in upper snake case, e.g. FOO_BAR"
  exit 1
fi

if [[ ! "$2" =~ ^[0-9]+$ ]]; then
  echo "Value for BugNumber ($2) is not a number"
  exit 1
fi

# Check for dirty files, mimicking what depot tools does.
# https://source.chromium.org/chromium/chromium/src/+/main:third_party/depot_tools/git_common.py;l=963;drc=4c050c6f1a34c7b1aaf503d97b871afb8540e54f
git update-index --refresh -q
git diff-index --ignore-submodules --name-status HEAD --
if [[ "$?" -ne 0 ]]; then
  echo 'Working directory has pending changes, aborting. Clean up first.'
  exit 1
fi

echo 'Creating new branch'
git new-branch "$BRANCH_NAME" &> /dev/null
if [[ "$?" -ne 0 ]]; then
  echo "Creating branch $BRANCH_NAME failed, aborting. If one already exists,
  delete it."
  exit 1
fi

echo 'Cherry-picking template CL'
git cl patch 5739234 &>/dev/null
# Avoid people uploading to the template CL.
git cl issue 0 &>/dev/null
if [[ "$?" -ne 0 ]]; then
  echo 'Cherry-picking template CL (https://crrev.com/c/5739234) failed,
  aborting. If the CL is healthy, rebase-update your checkout. Otherwise, ask
  components/sync/OWNERS to fix the CL'
  # Delete the new branch and go back to the previous one.
  git cherry-pick --abort
  git checkout --quiet -
  git branch --quiet -d "$BRANCH_NAME"
  exit 1
fi

echo 'Replacing template values with arguments'
upper_snake_case="$1"
lower_snake_case="$(to_lower_snake_case "$1")"
camel_case="$(to_camel_case "$1")"
title_case="$(to_title_case "$1")"
bug_number="$2"
git show HEAD --name-only --pretty='' | while read file; do
  # FOO_BAR and 123456789 are the DataType/BugNumber used by the template.
  replace_string_in_file "$file" 'FOO_BAR' "$upper_snake_case"
  replace_string_in_file "$file" 'foo_bar' "$lower_snake_case"
  replace_string_in_file "$file" 'FooBar' "$camel_case"
  replace_string_in_file "$file" 'Foo Bar' "$title_case"
  replace_string_in_file "$file" '123456789' "$2"
done
git mv components/sync/protocol/foo_bar_specifics.proto \
  "components/sync/protocol/${lower_snake_case}_specifics.proto"

echo 'Amending'
git commit --quiet -a --amend --no-edit

echo 'Success!'

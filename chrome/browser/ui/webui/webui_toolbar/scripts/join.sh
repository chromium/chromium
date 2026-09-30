#! /bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

function join() {
  local separator="$1"
  shift

  local old_ifs="$IFS"
  IFS="$separator"
  local result="$*"
  IFS="$old_ifs"
  echo "$result"
}

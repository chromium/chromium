#!/bin/bash -p
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: install.sh update_dmg_mount_point
#
# Exit codes:
#   0   Success!
#   99  This will return a usage message.
#   *   Other exit codes come from the updater --install exit codes.

PRODUCT_NAME=
readonly PRODUCT_NAME

usage() {
  echo "usage: ${ME} update_dmg_mount_point" >& 2
}

main() {
  local update_dmg_mount_point="${1}"
  local path_to_executable=\
"${update_dmg_mount_point}/${PRODUCT_NAME}.app/Contents/MacOS/${PRODUCT_NAME}"

  # Run the executable with --install
  if [[ $UPDATE_IS_MACHINE -eq 1 ]]; then
    "${path_to_executable}" --install --system
  else
    "${path_to_executable}" --install
  fi
}

# Check "less than" instead of "not equal to" in case there are changes to pass
# more arguments.
if [[ ${#} -lt 1 ]]; then
  usage
  echo ${#} >& 1
  exit 99
fi

main "${@}"
exit ${?}

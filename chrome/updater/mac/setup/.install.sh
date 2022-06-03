#!/bin/bash -p
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: install.sh update_dmg_mount_point installed_app_path current_version
#
# Called by Omaha v4 to install the updater.
#
# Exit codes:
#   0         Success!
#   10 - 42   Error messages from the install.
#   99        This will return a usage message.
#

# Set path to /bin, /usr/bin, /sbin, /usr/sbin
export PATH="/bin:/usr/bin:/sbin:/usr/sbin"

PRODUCT_NAME=
readonly PRODUCT_NAME
readonly APP_DIR="${PRODUCT_NAME}.app"

usage() {
  echo "usage: ${ME} update_dmg_mount_point" >& 2
}

main() {
  local update_dmg_mount_point="${1}"
  local path_to_executable=\
"${update_dmg_mount_point}/${APP_DIR}/Contents/MacOS/${PRODUCT_NAME}"

  # Run the executable with install
  "${path_to_executable}" --install
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

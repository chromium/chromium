#!/bin/bash
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -o pipefail
if [ "$VERBOSE" ]; then
  set -x
fi
set -u

# Remove temporary files and unwanted packaging output.
cleanup() {
  log_cmd echo "Cleaning..."
  rm -rf "${TMPFILEDIR}"
}

SCRIPTDIR=$(readlink -f "$(dirname "$0")")
OUTPUTDIR="${PWD}"
SNAPNAME="$1"
CHANNEL="$2"
VERSION="$3"
SNAP_ARCH="$4"
TARGET_OS="$5"

# call cleanup() on exit
trap cleanup 0

LOCKFILE="${OUTPUTDIR}/snap-lock"
TMPFILEDIR="${OUTPUTDIR}/snap-${CHANNEL}"
STAGEDIR="${TMPFILEDIR}/chrome"
mkdir -p "${STAGEDIR}"

cp "${OUTPUTDIR}/installer/version.txt" "${TMPFILEDIR}/"

source ${OUTPUTDIR}/installer/common/installer.include

if [ "$SNAPNAME" = "google-chrome" ]; then
  source "${OUTPUTDIR}/installer/common/google-chrome.info"
else
  source "${OUTPUTDIR}/installer/common/chromium-browser.info"
fi

prep_staging_common
PACKAGE_ORIG= # unused, but needs to be set
USR_BIN_SYMLINK_NAME= # unused, but needs to be set
SHLIB_PERMS=644
BRANDING="$(echo ${SNAPNAME} | tr '-' '_')"
stage_install_common
LAUNCHER_SCRIPT="${TMPFILEDIR}/chrome.launcher"
process_template "${SCRIPTDIR}/chrome.launcher.in" "${LAUNCHER_SCRIPT}"
chmod +x "${LAUNCHER_SCRIPT}"
process_template "${SCRIPTDIR}/snapcraft.yaml.in" "${TMPFILEDIR}/snapcraft.yaml"
if [ "$SNAPNAME" = "google-chrome" ]; then
  LOGO="product_logo_256"
  if [ "$CHANNEL" = "beta" ]; then
    sed -i -e "s:$LOGO.png:${LOGO}_beta.png:" "${TMPFILEDIR}/snapcraft.yaml"
  elif [ "$CHANNEL" = "unstable" ]; then
    sed -i -e "s:$LOGO.png:${LOGO}_dev.png:" "${TMPFILEDIR}/snapcraft.yaml"
  fi
fi

cd "${TMPFILEDIR}"

# Use flock to serialize all executions of snapcraft, as it currently
# doesn't handle well concurrent builds for the same snap name
# (https://bugs.launchpad.net/snapcraft/+bug/1869030).
flock "${LOCKFILE}" snapcraft

mv "${SNAPNAME}_${VERSION}_${SNAP_ARCH}.snap" \
  "${OUTPUTDIR}/${SNAPNAME}-${CHANNEL}_${VERSION}_${SNAP_ARCH}.snap"

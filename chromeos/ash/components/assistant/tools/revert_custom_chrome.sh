#!/usr/bin/env bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Counter-part to download_custom_chrome.sh. Restores the device to the previous
# non-custom chrome build that was running before download_custom_chrome.sh.
# This works even after multiple custom builds have been downloaded:
# * Running build A
# * download_custom_chrome.sh B
# * download_custom_chrome.sh C
# * restore_custom_chrome.sh returns the device to build A
#
# After the script finishes, the old build should be running if there were no
# errors. It should be run as the "root" user on the Chromebook itself. It can
# be run from any directory.

set -e

OLD_CHROME_ARCHIVE_DIR="/mnt/stateful_partition/old_chrome_archive"
CHROME_EXE_DIR="/opt/google/chrome"
CUSTOM_CHROME_FILE_INDICATOR="custom_chrome"

main() {
    if [ ! -f "$OLD_CHROME_ARCHIVE_DIR/chrome" ]; then
        echo "Old version of chrome to restore not found."
        exit 1
    fi

    stop ui || true
    cp -r ${OLD_CHROME_ARCHIVE_DIR}/* $CHROME_EXE_DIR
    # The custom chrome file indicator may still be there from the previous
    # custom chrome build. Remove it if it exists since the original non-custom
    # build is being restored.
    rm -f ${CHROME_EXE_DIR}/${CUSTOM_CHROME_FILE_INDICATOR}
    start ui

    rm -rf $OLD_CHROME_ARCHIVE_DIR
}


main "$@"

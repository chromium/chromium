#!/usr/bin/env bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Use the stateful partition to download the new custom chrome image because it
# has more space.
CUSTOM_CHROME_DIR="/mnt/stateful_partition/custom_chrome"
# Archives the non-custom chrome build that was running before
# download_custom_chrome.sh. This is so that it can be restored in the future if
# desired via revert_custom_chrome.sh.
OLD_CHROME_ARCHIVE_DIR="/mnt/stateful_partition/old_chrome_archive"
CHROME_EXE_DIR="/opt/google/chrome"
CUSTOM_CHROME_FILE_INDICATOR="custom_chrome"

main() {
    local arg_count=$#
    if [ $arg_count -lt 1 ]; then
        print_help
        exit 1
    fi

    clear_and_create_directory $CUSTOM_CHROME_DIR
    cd $CUSTOM_CHROME_DIR
    curl -L --progress-bar https://storage.googleapis.com/chromeos-throw-away-bucket/easy-chrome/${1}/chrome.tar.zst | tar --zstd -xvf -
    # Use a blank file as an indicator that this is a custom chrome build. This
    # helps if download_custom_chrome.sh is run again because the indicator
    # tells the script that a custom chrome build is already running in
    # /opt/google/chrome, so it should not be archived in the
    # OLD_CHROME_ARCHIVE_DIR.
    touch $CUSTOM_CHROME_DIR/$CUSTOM_CHROME_FILE_INDICATOR

    stop ui || true

    if [ ! -f "$CHROME_EXE_DIR/$CUSTOM_CHROME_FILE_INDICATOR" ]; then
        clear_and_create_directory $OLD_CHROME_ARCHIVE_DIR
        cp -r ${CHROME_EXE_DIR}/* $OLD_CHROME_ARCHIVE_DIR
    fi

    cp -r ${CUSTOM_CHROME_DIR}/* $CHROME_EXE_DIR

    start ui

    rm -rf $CUSTOM_CHROME_DIR
}

print_help() {
    cat <<END_OF_HELP

Usage: download_custom_chrome.sh <easy-chrome bucket>

Example: download_custom_chrome.sh esum_KNG60

Downloads and runs a custom chrome build that has been uploaded to
https://storage.googleapis.com/chromeos-throw-away-bucket/easy-chrome/...

The script's only argument dictates the path within the storage bucket.
In the example above, the full url would become:
https://storage.googleapis.com/chromeos-throw-away-bucket/easy-chrome/esum_KNG60/chrome.tar.zst

Note it is recommended to add several randomly generated characters to the end
of your bucket name as in the example above.

After the script finishes, the new build should be running if there were no
errors. It should be run as the "root" user on the Chromebook itself. It can be
run from any directory.
END_OF_HELP

}

clear_and_create_directory() {
    rm -rf $1
    mkdir -p $1
}


main "$@"

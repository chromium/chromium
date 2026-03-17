#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

print_usage() {
    cat << 'EOF'
==============================================================================
Usage:
./test_extension_setup.sh [-h] [-C <build_dir>] [-d <remote_extension_dir>]

This script configures the environment and connected Android device for
Desktop Android testing. It automates the following steps:
  1. Validates the Chromium build config (requires is_desktop_android = true).
  2. Verifies an active ADB device connection.
  3. Builds and installs the Chrome APK, and sets it as the default browser.
  4. Builds and installs the CustomTabsClientExample APK.
  5. Pushes the local 'test_extension' folder to the device.

Options:
  -h, --help
      Show this help message and exit.

  -C <build_dir>
      Path to the Chromium build directory.
      Default: out/Default

  -d <remote_extension_dir>
      Path on the Android device where the test extension should be pushed.
      Provide this if your device uses a specific user profile path
      (e.g., /mnt/user/10/emulated/10/test_extension).
      Default: /sdcard/Download/test_extension
==============================================================================
EOF
}

setup() {
    local build_dir="out/Default"
    local remote_ext_dir="/sdcard/Download/test_extension"
    local OPTIND opt

    # Parse the flags.
    if [[ "$1" == "--help" ]]; then
        print_usage
        return 0
    fi

    while getopts "hC:d:" opt; do
        case "$opt" in
            C) build_dir=$OPTARG ;;
            d) remote_ext_dir=$OPTARG ;;
            h) print_usage; return 0 ;;
            *) echo "Usage: test_extension_setup.sh [-C <build_dir>] " \
                    "[-d <remote_extension_dir>]"; return 1 ;;
        esac
    done
    shift $((OPTIND - 1))

    # Check existence of build directory.
    if [[ -d "$build_dir" ]]; then
        build_dir=$(realpath "$build_dir")
    else
        echo "Error: Directory '$build_dir' does not exist."
        return 1
    fi

    if [[ ! -f "$build_dir/args.gn" ]]; then
        echo "Warning: $build_dir does not look like a Chromium " \
             "build output directory (missing args.gn)."
    fi
    echo "Validating build configuration in $build_dir..."

    # Check that is_desktop_android is set to true.
    RAW_GN_ARGS=$(gn args "$build_dir" --list=is_desktop_android \
                  --short 2>/dev/null)
    IS_DESKTOP=$(echo "$RAW_GN_ARGS" | cut -d'=' -f2 | xargs)

    if [[ "$IS_DESKTOP" != "true" ]]; then
        echo "Error: This build is not configured for Desktop Android."
        echo "Please ensure 'is_desktop_android = true' is in your args.gn"
        return 1
    fi
    echo "Build configuration verified: is_desktop_android = true"

    # Check that a device is connected.
    if ! adb get-state &>/dev/null; then
        echo "Error: No device connected or authorized."
        return 1
    fi
    echo "Device connection verified."

    # Build and install Chrome.
    autoninja -C "$build_dir" chrome_apk || return 1
    adb install -r "$build_dir/apks/Chrome.apk"
    # Set default browser.
    PACKAGE_NAME="com.google.android.apps.chrome"
    adb shell cmd role add-role-holder android.app.role.BROWSER $PACKAGE_NAME
    # Build and install CCT app.
    autoninja -C "$build_dir" custom_tabs_client_example_apk || return 1
    adb install -r "$build_dir/apks/CustomTabsClientExample.apk"

    # Copy extension code to device.
    LOCAL_EXT_DIR="$(dirname "${BASH_SOURCE[0]}")/test_extension"
    # Confirm if the local extension directory actually exists
    if [[ -d "$LOCAL_EXT_DIR" ]]; then
        echo "Pushing test extension to device..."

        # Make sure the extension directory exists and is empty.
        adb shell rm -rf "$remote_ext_dir"
        adb shell mkdir -p "$remote_ext_dir"

        # Push the extension to the device.
        adb push "$LOCAL_EXT_DIR/." "$remote_ext_dir/"

        echo "Test Extension files synced to $remote_ext_dir"
    else
        echo "Warning: '$LOCAL_EXT_DIR' not found. Skipping extension sync." \
        "Was this script moved?"
    fi
}

setup "$@"

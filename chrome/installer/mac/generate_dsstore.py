#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to programmatically generate .DS_Store files for macOS DMGs.

This script uses the 'ds_store' and 'mac_alias' libraries from third_party
to create a .DS_Store file with a background image (via Alias record) and
positioned icons. It runs on both Linux and macOS.
"""

import argparse
import datetime
import os
import sys

# Get the absolute path of the directory containing this script.
# This script will be in chrome/installer/mac.
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Path to src/ root.
_SRC_ROOT = os.path.abspath(os.path.join(_SCRIPT_DIR, "../../.."))

# Paths to vendored libraries.
_DS_STORE_PATH = os.path.join(_SRC_ROOT, "third_party/ds_store/src/src")
_MAC_ALIAS_PATH = os.path.join(_SRC_ROOT, "third_party/mac_alias/src/src")

# Add them to sys.path.
sys.path.insert(0, _DS_STORE_PATH)
sys.path.insert(0, _MAC_ALIAS_PATH)

try:
    from ds_store import DSStore
    from mac_alias import (
        Alias,
        TargetInfo,
        VolumeInfo,
        ALIAS_KIND_FILE,
        ALIAS_EJECTABLE_DISK,
        ALIAS_HFS_VOLUME_SIGNATURE,
        ALIAS_NO_CNID,
    )
except ImportError as e:
    print(f"Error importing libraries from //third_party: {e}",
          file=sys.stderr)
    sys.exit(1)

# Finder background type constants.
FINDER_BACKGROUND_TYPE_COLOR = 1
FINDER_BACKGROUND_TYPE_PICTURE = 2


def main():
    parser = argparse.ArgumentParser(
        description="Generate .DS_Store file for macOS DMG installers")
    parser.add_argument("--output",
                        required=True,
                        help="Path to output .DS_Store file")
    parser.add_argument("--volume-name",
                        required=True,
                        help="Volume name of the DMG")
    parser.add_argument(
        "--app-name",
        required=True,
        help="Name of the App bundle, e.g., 'Google Chrome.app'",
    )
    parser.add_argument(
        "--background-image",
        default="/.background/background.png",
        help="Path to background image inside DMG",
    )

    # Geometry arguments
    parser.add_argument("--window-width", type=int, default=480)
    parser.add_argument("--window-height", type=int, default=540)
    parser.add_argument("--window-x", type=int, default=240)
    parser.add_argument("--window-y", type=int, default=180)

    parser.add_argument("--icon-size", type=int, default=128)
    parser.add_argument("--text-size", type=int, default=12)

    # Icon positions
    parser.add_argument("--app-x", type=int, default=240)
    parser.add_argument("--app-y", type=int, default=122)
    parser.add_argument("--link-x", type=int, default=240)
    parser.add_argument("--link-y", type=int, default=387)

    args = parser.parse_args()

    # 1. Construct the Alias record for the background image.
    # Use a fixed creation date to ensure that the output is deterministic.
    reproducible_date = datetime.datetime(2000,
                                          5,
                                          7,
                                          21,
                                          10,
                                          0,
                                          tzinfo=datetime.timezone.utc)

    volume = VolumeInfo(
        name=args.volume_name,
        creation_date=reproducible_date,
        fs_type=ALIAS_HFS_VOLUME_SIGNATURE,
        disk_type=ALIAS_EJECTABLE_DISK,
        attribute_flags=0,
        fs_id=b"\x00\x00",
        posix_path=f"/Volumes/{args.volume_name}",
    )

    bg_filename = os.path.basename(args.background_image)
    bg_parent = os.path.dirname(args.background_image)

    target = TargetInfo(
        kind=ALIAS_KIND_FILE,
        filename=bg_filename,
        folder_cnid=ALIAS_NO_CNID,
        cnid=ALIAS_NO_CNID,
        creation_date=reproducible_date,
        creator_code=b"\x00\x00\x00\x00",
        type_code=b"\x00\x00\x00\x00",
        folder_name=None,
        posix_path=args.background_image,
        carbon_path=
        f"{args.volume_name}:{bg_parent.lstrip('/').replace('/', ':')}:{bg_filename}",
    )

    alias = Alias(volume=volume, target=target)
    alias_bytes = alias.to_bytes()

    # 2. Construct the DSStore file.
    bounds_string = f"{{{{{args.window_x}, {args.window_y}}}, {{{args.window_width}, {args.window_height}}}}}"

    bwsp = {
        "ShowStatusBar": False,
        "WindowBounds": bounds_string,
        "ContainerShowSidebar": False,
        "PreviewPaneVisibility": False,
        "SidebarWidth": 180,
        "ShowTabView": False,
        "ShowToolbar": False,
        "ShowPathbar": False,
        "ShowSidebar": False,
    }

    icvp = {
        "viewOptionsVersion": 1,
        "backgroundType": FINDER_BACKGROUND_TYPE_PICTURE,
        "backgroundImageAlias": alias_bytes,
        "gridOffsetX": 0.0,
        "gridOffsetY": 0.0,
        "gridSpacing": 100.0,
        "arrangeBy": "none",
        "showIconPreview": True,
        "showItemInfo": False,
        "labelOnBottom": True,
        "textSize": float(args.text_size),
        "iconSize": float(args.icon_size),
        "backgroundColorRed": 1.0,
        "backgroundColorGreen": 1.0,
        "backgroundColorBlue": 1.0,
    }

    # 'icvl' specifies the default view. It's value uses a codec which is not
    # registered with the `ds_store` library. It must be specified explicitly.
    # 1. The type is 'type' (a macOS four-character code type)
    # 2. The value is 'icnv' (the four-character code for Icon View)
    icvl = (b"type", b"icnv")

    # Write the DS_Store file.
    with DSStore.open(args.output, "w+") as d:
        # 'vSrn' is version, not in codecs, requires explicit type 'long'.
        d["."]["vSrn"] = ("long", 1)
        d["."]["bwsp"] = bwsp
        d["."]["icvp"] = icvp
        d["."]["icvl"] = icvl

        # Position the App icon.
        d[args.app_name]["Iloc"] = (args.app_x, args.app_y)

        # Position the Applications symlink (named with a single space ' ').
        d[" "]["Iloc"] = (args.link_x, args.link_y)

    print(f"Successfully generated DS_Store at {args.output}")


if __name__ == "__main__":
    main()

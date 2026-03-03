#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import itertools
import os
import pathlib
import sys

from signing import rebranding

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Rebrand Chromium DMGs with Omaha tags.")
    parser.add_argument(
        "--hfs-tool",
        required=True,
        type=pathlib.Path,
        help="Path to the hfs_tool binary. Required.")
    parser.add_argument(
        "--dmg-tool",
        required=True,
        type=pathlib.Path,
        help="Path to the dmg_tool binary. Required.")
    parser.add_argument(
        "--source-dmg",
        required=True,
        type=pathlib.Path,
        help="Path to the source DMG to rebrand.")
    parser.add_argument(
        "--output-dir",
        required=True,
        type=pathlib.Path,
        help="Directory where branded DMGs will be saved. It will be created"
        " if necessary. If it is not empty, --stomp_ok is required and any"
        " conflicting files will be overwritten.")
    parser.add_argument(
        "--stomp-ok",
        action="store_true",
        help="Allow using an output directory that already exists and contains"
        " files. Existing items will be overwritten without notice.")
    parser.add_argument(
        "--brand",
        action="append",
        dest="brand_codes",
        required=True,
        help="Brand code to apply, or multiple brand codes comma-separated."
        " Can be specified multiple times. Syntaxes can be combined.")
    parser.add_argument(
        "--app-name",
        default="Google Chrome",
        help="Name of the application bundle without the .app extension "
        "(default: Google Chrome).")
    parser.add_argument(
        "--version",
        default="0.0.0.0",
        help="Version of the application (default: 0.0.0.0).")
    parser.add_argument(
        "--channel",
        default="Manual",
        help="Channel of the application (default: Manual).")

    args = parser.parse_args()

    # Normalize brand codes.
    brand_codes = itertools.chain.from_iterable(
        s.split(",") for s in args.brand_codes)
    brand_codes = (s.strip() for s in brand_codes)
    brand_codes = [s for s in brand_codes if s]

    # Find or build tools invoked by this script.
    if not args.hfs_tool or not args.dmg_tool:
        print(
            "Either --hfs-tool and --dmg-tool must be provided.",
            file=sys.stderr)
        sys.exit(1)

    hfs_tool = rebranding.HfsTool(args.hfs_tool)
    dmg_tool = rebranding.DmgTool(args.dmg_tool)

    # Create output directory.
    try:
        os.makedirs(args.output_dir, exist_ok=args.stomp_ok)
    except FileExistsError:
        if not os.path.isdir(args.output_dir):
            print(
                f"Error: Output directory {args.output_dir} exists but is not "
                "a directory.",
                file=sys.stderr)
            sys.exit(1)
        if os.listdir(args.output_dir):
            print(
                f"Error: Output directory {args.output_dir} already exists.",
                file=sys.stderr)
            print(
                f"To use it anyway, overwriting conflicting files, use "
                "--stomp-ok.",
                file=sys.stderr)
            sys.exit(1)

    rebranding.rebrand_chromium_dmg(
        hfs_tool=hfs_tool,
        dmg_tool=dmg_tool,
        source_dmg_path=args.source_dmg,
        output_dir=args.output_dir,
        brand_codes=brand_codes,
        app_name=args.app_name,
        version=args.version,
        channel=args.channel)

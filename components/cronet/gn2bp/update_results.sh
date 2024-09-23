#!/bin/bash

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is expected to run after gen_android_bp is modified.
#
#   ./update_result.sh
#
# TARGETS contains targets which are supported by gen_android_bp and
# this script generates Android.bp.swp from TARGETS.
# This makes it easy to realize unintended impact/degression on
# previously supported targets.

set -eux

BASEDIR=$(dirname "$0")
# Run the licensing script to update all the METADATA / LICENSE files.
python3 $BASEDIR/../license/create_android_metadata_license.py && \
python3 $BASEDIR/gen_android_bp.py \
    --desc $BASEDIR/desc_x64.json \
    --desc $BASEDIR/desc_x86.json \
    --desc $BASEDIR/desc_arm.json \
    --desc $BASEDIR/desc_arm64.json \
    --desc $BASEDIR/desc_riscv64.json \
    --out $BASEDIR/Android.bp \
    --repo_root "$1"

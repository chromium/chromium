#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)
OUT_DIR=$CHROMIUM_SRC/components/test/data/feed

if [ ! -d $OUT_DIR ]; then
  echo "Output directory $OUT_DIR doesn't exist."
  exit 1
fi

python3 $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
  --chromium_path=$CHROMIUM_SRC \
  --output_file=$OUT_DIR/response.binarypb \
  --source_file=\
$CHROMIUM_SRC/components/feed/core/v2/testdata/response.textproto

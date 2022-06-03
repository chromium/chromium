#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage: cat request_binary_file | ./decode_binary_feed_request.py

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)
python3 \
 $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
   --direction=reverse --chromium_path="$CHROMIUM_SRC" \
   --message=feedwire.Request

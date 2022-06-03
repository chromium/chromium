#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage: make_feed_query_request.sh request.textproto

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)

PLD=$(python3 \
  $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
  --chromium_path=$CHROMIUM_SRC \
  --message=feedwire.Request \
  --output_format=base64 \
  --source_file=$1)

BASE_URL="https://www.google.com/httpservice"
ENDPOINT="TrellisClankService/FeedQuery"
QUERY_URL="$BASE_URL/retry/$ENDPOINT"

echo "$QUERY_URL?fmt=bin&hl=en-US&reqpld=$PLD"

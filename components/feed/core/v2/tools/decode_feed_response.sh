#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Converts a Feed HTTP response from binary to text using proto definitions from
# Chromium.
#
# Usage: curl 'some url' | feed_response_to_textproto.sh

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)

# Responses start with a varint length value that must be removed.
python3 -c "import sys
while sys.stdin.buffer.read(1)[0]>127:
  pass
sys.stdout.buffer.write(sys.stdin.buffer.read())" | \
python3 $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
  --chromium_path=$CHROMIUM_SRC --direction=reverse

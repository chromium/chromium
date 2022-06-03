#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Converts a Feed response textproto to a binary proto.
#
# Usage: cat some.textproto | feed_response_to_textproto.sh > result.binarypb

CHROMIUM_SRC=$(realpath $(dirname $(readlink -f $0))/../../../../..)

python3 $CHROMIUM_SRC/components/feed/core/v2/tools/textpb_to_binarypb.py \
  --chromium_path=$CHROMIUM_SRC --direction=forward

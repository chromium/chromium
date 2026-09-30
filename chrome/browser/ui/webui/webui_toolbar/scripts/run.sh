#! /bin/bash -ex
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run chrome with a set of flags
#
# run.sh <binary> [any other flags you want]
#
# E.g. ./run.sh google-chrome --other-chrome-arguments

binary=$1
shift

"$binary" --enable-features="$($(dirname "$0")/enabled.sh)" "$@"

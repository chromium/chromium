#!/bin/bash -p
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Exit codes come from the updater exit codes.

PRODUCT_NAME=
readonly PRODUCT_NAME

env

"$1/${PRODUCT_NAME}.app/Contents/MacOS/${PRODUCT_NAME}" \
    ${SERVER_ARGS} ${KS_kServerActionArguments}

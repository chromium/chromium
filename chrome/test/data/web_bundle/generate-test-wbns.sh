#!/bin/sh

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

# Generate a WBN which includes urn:uuid resources.
gen-bundle \
  -version b1 \
  -har urn-handler-test.har \
  -primaryURL urn:uuid:be40faa5-b581-42af-9c0c-fd9cb3b8a7a0 \
  -o urn-handler-test.wbn

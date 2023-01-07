#!/bin/sh

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

for cmd in gen-bundle sign-bundle; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
        echo '  export PATH=$PATH:$(go env GOPATH)/bin'
        exit 1
    fi
done

gen-bundle \
  -version b2 \
  -baseURL isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/ \
  -primaryURL isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/ \
  -dir simple_web_bundle/ \
  -o simple.wbn

sign-bundle \
  -i simple.wbn \
  -signType integrityblock \
  -privateKey signed_web_bundle_private_key.pem \
  -o simple_signed.wbn

# Rewrite "<h1>Simple Web Bundle</h1>" to "<h1>Tampered Web Bundle</h1>" so that
# the signature should no longer be valid.
xxd -p simple_signed.wbn |
  tr -d '\n' |
  sed 's/53696d706c65/54616d7065726564/' |
  xxd -r -p > simple_signed_tampered.wbn

rm simple.wbn

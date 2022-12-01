#!/bin/sh

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

for cmd in gen-bundle sign-bundle; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go install github.com/WICG/webpackage/go/bundle/cmd/...@latest"
        echo '  export PATH=$PATH:$(go env GOPATH)/bin'
        exit 1
    fi
done

sxg_test_data_dir=../../../../content/test/data/sxg
signature_date=2019-07-28T00:00:00Z

gen-bundle \
  -version b2 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir hello/ \
  -o hello_b2.wbn

gen-bundle \
  -version b2 \
  -har mixed_absolute_relative_urls.har \
  -o mixed_absolute_relative_urls.wbn

gen-bundle \
  -version b2 \
  -har simple.har \
  -o simple_b2.wbn

gen-bundle \
  -version b2 \
  -har 24_responses.har \
  -o 24_responses.wbn

sign-bundle signatures-section \
  -i hello_b2.wbn \
  -certificate $sxg_test_data_dir/test.example.org.public.pem.cbor \
  -privateKey $sxg_test_data_dir/prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -validityUrl https://test.example.org/resource.validity.msg \
  -o hello_vouched_subsets.wbn

sign-bundle integrity-block \
  -i simple_b2.wbn \
  -privateKey signed_web_bundle_private_key.pem \
  -o simple_b2_signed.swbn

# Rewrite "Hello Web Bundles" to "Tampered Web Bundles" so that the signature
# should no longer be valid.
xxd -p simple_b2_signed.swbn |
  tr -d '\n' |
  sed 's/48656c6c6f/54616d7065726564/' |
  xxd -r -p > simple_b2_signed_tampered.swbn

if cmp -s "simple_b2_signed.swbn" "simple_b2_signed_tampered.swbn"; then
    echo "Error: Tampered version is equal to the untampered version."
    exit 1
fi

#!/bin/bash

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates the following tree of certificates:
#     root (self-signed root)
#      \
#       \--> l1_leaf (end-entity)

SRC_DIR="../../../../../.."
export CA_CERT_UTIL_DIR="${SRC_DIR}/chrome/test/data/policy/ca_util"
source "${CA_CERT_UTIL_DIR}/ca_util.sh"
export CA_CERT_UTIL_OUT_DIR="./out/"

try rm -rf out
try mkdir out

CN=root \
  try root_cert root

CA_ID=root CN=l1_leaf \
  try issue_cert l1_leaf leaf_cert_without_san as_der as_pk8

try rm -rf out

# Also print out the fingerprint to be put into certificate_provider_apitest.cc.
# The fingerprint is in the format AA:BB:CC:... but the c++ side expects aabbcc
# so convert to lowercase using awk and then remove colons using sed.
echo "Make sure to update the fingerprint certificate_provider_apitest.cc"
echo "The new fingerprint of l1_leaf.der is:"
openssl x509 -inform DER -noout -fingerprint -in l1_leaf.der \
  | awk '{print tolower($0)}' | sed -e "s/://g"

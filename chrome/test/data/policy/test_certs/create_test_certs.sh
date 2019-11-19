#!/bin/bash

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates the following tree of certificates:
#     root_ca_cert (self-signed root)
#      \
#       \--> ok_cert (end-entity)
#      \
#       \--> intermediate_ca_cert (intermediate CA)
#        \
#         \--> ok_cert_by_intermediate (end-identity)

SRC_DIR="../../../../.."
export CA_CERT_UTIL_DIR="${SRC_DIR}/chrome/test/data/policy/ca_util"
source "${CA_CERT_UTIL_DIR}/ca_util.sh"
export CA_CERT_UTIL_OUT_DIR="./out/"

try rm -rf out
try mkdir out

CN=root_ca_cert \
  try root_cert root_ca_cert

CA_ID=root_ca_cert CN="127.0.0.1" \
  try issue_cert ok_cert leaf_cert_san_ip as_pem

CA_ID=root_ca_cert CN=intermediate_ca_cert \
  try issue_cert intermediate_ca_cert ca_cert as_pem

CA_ID=intermediate_ca_cert CN="127.0.0.1" \
  try issue_cert ok_cert_by_intermediate leaf_cert_san_ip as_pem

try rm -rf out

# Read the root CA cert and interemdiate CA cert PEM files and replace newlines
# with \n literals. This is needed because the ONC JSON does not support
# multi-line strings. Note that replacement is done in two steps, using ',' as
# intermediate character. PEM files will not contain commas.
ROOT_CA_CERT_CONTENTS=$(cat root_ca_cert.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')
INTERMEDIATE_CA_CERT_CONTENTS=$(cat intermediate_ca_cert.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')

cat > root-ca-cert.onc << EOL
{
  "Certificates": [
    {
      "GUID": "{b3aae353-cfa9-4093-9aff-9f8ee2bf8c29}",
      "TrustBits": [
        "Web"
      ],
      "Type": "Authority",
      "X509": "${ROOT_CA_CERT_CONTENTS}"
    }
  ],
  "Type": "UnencryptedConfiguration"
}
EOL

cat > root-and-intermediate-ca-certs.onc << EOL
{
  "Certificates": [
    {
      "GUID": "{b3aae353-cfa9-4093-9aff-9f8ee2bf8c29}",
      "TrustBits": [
        "Web"
      ],
      "Type": "Authority",
      "X509": "${ROOT_CA_CERT_CONTENTS}"
    },
    {
      "GUID": "{ac861420-3342-4537-a20e-3c2ec0809b7a}",
      "TrustBits": [ ],
      "Type": "Authority",
      "X509": "${INTERMEDIATE_CA_CERT_CONTENTS}"
    }
  ],
  "Type": "UnencryptedConfiguration"
}
EOL

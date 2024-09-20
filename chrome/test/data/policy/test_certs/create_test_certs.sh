#!/bin/bash

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates the following tree of certificates using RSA keys:
#     root_ca_cert (self-signed root)
#      \
#       \--> ok_cert (end-entity)
#      \
#       \--> intermediate_ca_cert (intermediate CA)
#        \
#         \--> ok_cert_by_intermediate (end-identity)
#
# As well as the following tree of certificates using EC keys:
#     root_ca_cert_ec (self-signed root)
#      \
#       \--> ok_cert_ec (end-entity)
#      \
#       \--> intermediate_ca_cert_ec (intermediate CA)
#        \
#         \--> ok_cert_by_intermediate_ec (end-identity)
#
# The RSA and EC certificates are independent of each other and only one
# should be used, this script generates both for convenience.

SRC_DIR="../../../../.."
export CA_CERT_UTIL_DIR="${SRC_DIR}/chrome/test/data/policy/ca_util"
source "${CA_CERT_UTIL_DIR}/ca_util.sh"
export CA_CERT_UTIL_OUT_DIR="./out/"

try rm -rf out
try mkdir out

CN=root_ca_cert \
  try root_cert_rsa root_ca_cert

CA_ID=root_ca_cert CN="127.0.0.1" SAN="IP:127.0.0.1" \
  try issue_cert_rsa ok_cert leaf_cert_san as_pem

CA_ID=root_ca_cert CN=intermediate_ca_cert_rsa \
  try issue_cert_rsa intermediate_ca_cert ca_cert as_pem

CA_ID=intermediate_ca_cert CN="127.0.0.1" SAN="IP:127.0.0.1" \
  try issue_cert_rsa ok_cert_by_intermediate leaf_cert_san as_pem

CN=root_ca_cert_ec \
  try root_cert_ec root_ca_cert_ec

CA_ID=root_ca_cert_ec CN="127.0.0.1" SAN="IP:127.0.0.1" \
  try issue_cert_ec ok_cert_ec leaf_cert_san as_pem

CA_ID=root_ca_cert_ec CN=intermediate_ca_cert_ec \
  try issue_cert_ec intermediate_ca_cert_ec ca_cert as_pem

CA_ID=intermediate_ca_cert_ec CN="127.0.0.1" SAN="IP:127.0.0.1" \
  try issue_cert_ec ok_cert_by_intermediate_ec leaf_cert_san as_pem

try rm -rf out

# Read the root CA cert and interemdiate CA cert PEM files and replace newlines
# with \n literals. This is needed because the ONC JSON does not support
# multi-line strings. Note that replacement is done in two steps, using ',' as
# intermediate character. PEM files will not contain commas.
# This is done for both RSA and EC certificates.
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

ROOT_CA_CERT_CONTENTS=$(cat root_ca_cert_ec.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')
INTERMEDIATE_CA_CERT_CONTENTS=$(cat intermediate_ca_cert_ec.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')

cat > root-ca-cert_ec.onc << EOL
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

cat > root-and-intermediate-ca-certs_ec.onc << EOL
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

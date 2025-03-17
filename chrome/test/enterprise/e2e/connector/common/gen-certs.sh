#!/bin/bash

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate certificates and keys for testing client and root certificates.
#
# This script should be run manually when the certificates are near expiration.

if [ -z "$1" ]; then
  echo "usage: $0 <gcs-bucket-name>" && exit 1
fi
for bin in mktemp gcloud openssl; do
  which $bin >/dev/null || (echo "missing executable: $bin" && exit 1)
done

set -x
out=$(mktemp -d)

openssl req -x509 -noenc -days 1825 \
  -newkey rsa:4096 -keyout $out/server-ca.key -out $out/server-ca.pem \
  -subj "/C=US/O=Chrome/OU=Chrome Enterprise"

cat <<'EOF' >"$out/server.ext"
[req]
default_bits              = 4096
req_extensions            = extension_requirements
distinguished_name        = dn_requirements
prompt                    = no

[extension_requirements]
basicConstraints          = CA:FALSE
keyUsage                  = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage          = serverAuth
subjectAltName            = @sans_list

[dn_requirements]
countryName               = US
0.organizationName        = Chrome
organizationalUnitName    = Chrome Enterprise
commonName                = test1.com

[sans_list]
DNS.1                     = test1.com
EOF

openssl req -new -noenc -newkey rsa:4096 \
  -keyout $out/server.key -out $out/server.csr -config $out/server.ext
openssl x509 -req -days 1825 \
  -extfile $out/server.ext -extensions extension_requirements \
  -CA $out/server-ca.pem -CAkey $out/server-ca.key -CAcreateserial \
  -in $out/server.csr -out $out/server.pem
openssl x509 -in $out/server.pem -text

# Remove any holds in case we're overwriting existing objects.
gcloud storage objects update gs://$1/secrets/certs/'*' --no-event-based-hold
gcloud storage cp $out/*.key $out/*.pem gs://$1/secrets/certs
# Restore the holds so that the objects remain available indefinitely.
gcloud storage objects update gs://$1/secrets/certs/'*' --event-based-hold
rm -rf $out

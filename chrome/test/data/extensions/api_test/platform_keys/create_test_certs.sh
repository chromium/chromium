#!/bin/bash

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates the following trees of certificates:
#
#     client_root (self-signed root)
#     \   \   \
#      \   \   \-> client_1_ca --> client_1 (end-entity)
#       \   \
#        \   \---> client_2_ca --> client_2 (end-entity)
#         \
#          \-----> client_3_ca --> client_3 (end-entity)
#
#     root (self-signed root)
#     \   \
#      \   \--> l1_leaf (end-entity)
#       \
#        \----> l1_interm --> l2_leaf (end-entity)

SRC_DIR="../../../../../.."
export CA_CERT_UTIL_DIR="${SRC_DIR}/chrome/test/data/policy/ca_util"
source "${CA_CERT_UTIL_DIR}/ca_util.sh"
export CA_CERT_UTIL_OUT_DIR="./out/"

try rm -rf out
try mkdir out


# Create client_root

try openssl genrsa -out out/client_root.key 2048

COMMON_NAME="Client Root CA" \
  ID=client_root \
  try openssl req \
    -new \
    -key out/client_root.key \
    -config client-certs.cnf \
    -out out/client_root.csr

COMMON_NAME="Client Root CA" \
  ID=client_root \
  try openssl x509 \
    -req -days 3650 \
    -in out/client_root.csr \
    -extensions ca_cert \
    -extfile client-certs.cnf \
    -signkey out/client_root.key \
    -out out/client_root.pem

echo 1000 > out/client_root-serial

touch out/client_root-index.txt

# Generate CA keys
try openssl genrsa -out "out/client_1_ca.key" 2048
try openssl genrsa -out "out/client_2_ca.key" 2048
try openssl ecparam -out out/client_3_ca.key -name prime256v1 -genkey

# Generate client keys
try openssl genrsa -out "out/client_1.key" 2048
try openssl genrsa -out "out/client_2.key" 2048
try openssl ecparam -out out/client_3.key -name prime256v1 -genkey

for i in 1 3
do

  # Create client_{1,2,3}_ca

  COMMON_NAME="Client Cert ${i} CA" \
    ID="client_${i}_ca" \
    try openssl req \
      -new \
      -key "out/client_${i}_ca.key" \
      -config client-certs.cnf \
      -out "out/client_${i}_ca.csr"

  COMMON_NAME="Client Cert ${i} CA" \
    ID=client_root \
    try openssl ca \
      -batch \
      -extensions ca_cert \
      -in "out/client_${i}_ca.csr" \
      -config client-certs.cnf \
      -out "out/client_${i}_ca.pem"

  echo $(expr 1000 + ${i}) > "out/client_${i}_ca-serial"

  touch "out/client_${i}_ca-index.txt"


  # Create client_{1,2,3}

  COMMON_NAME="Client Cert ${i}" \
    ID="client_${i}" \
    try openssl req \
      -new \
      -key "out/client_${i}.key" \
      -config client-certs.cnf \
      -out "out/client_${i}.csr"

  COMMON_NAME="Client Cert ${i}" \
    ID="client_${i}_ca" \
    try openssl ca \
      -batch \
      -extensions user_cert \
      -in "out/client_${i}.csr" \
      -config client-certs.cnf \
      -out "client_${i}.pem"

  try openssl pkcs8 \
    -topk8 -nocrypt \
    -in "out/client_${i}.key" \
    -outform DER \
    -out "client_${i}.pk8"

  try openssl x509 \
    -in "client_${i}.pem" \
    -outform DER \
    -out "client_${i}.der"

done


# Create additional files for client_1

try openssl rsa \
  -in "out/client_1.key" \
  -inform PEM \
  -pubout \
  -outform DER \
  -out client_1_spki.der

try openssl asn1parse \
  -in client_1.der \
  -inform DER \
  -strparse 32 \
  -out client_1_issuer_dn.der


# Create signatures using client_{1,2}

for i in 1 2
do
  try openssl dgst \
    -sha1 \
    -sign "out/client_${i}.key" \
    -out "signature_client${i}_sha1_pkcs" \
    data
done

try openssl rsautl \
  -inkey "out/client_1.key" \
  -sign \
  -in data \
  -pkcs \
  -out signature_nohash_pkcs

# Generate an EC P-256 private key
try openssl ecparam \
  -genkey \
  -name prime256v1 \
  -out out/ec_private_key.der \
  -outform der

# Extract the SPKI from the private key
try openssl ec \
  -pubout \
  -in out/ec_private_key.der \
  -inform der \
  -out ec_spki.der \
  -outform der

# Generate certificate for ec_private_key
try openssl req \
  -new \
  -x509 \
  -key out/ec_private_key.der \
  -keyform der \
  -out ec_cert.der \
  -outform der

# Create root, l1_interm, l{1,2}_leaf

CN=root \
  try root_cert root

CA_ID=root CN=l1_leaf SAN="DNS:${CN}"\
  try issue_cert l1_leaf leaf_cert_san as_der

CA_ID=root CN=l1_interm \
  try issue_cert l1_interm ca_cert as_der

CA_ID=l1_interm CN=l2_leaf SAN="DNS:${CN}"\
  try issue_cert l2_leaf leaf_cert_san as_der

try rm -rf out

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Provides functions root_cert() and issue_cert() to create certificate
# hierarchies.

# All functions expect the envorinment variables $CA_CERT_UTIL_DIR and
# $CA_CERT_UTIL_OUT_DIR to be set.
#
# $CA_CERT_UTIL_DIR should be set to the directory where this utility script
# resides in, so it can find auxiliary files.
#
# $CA_CERT_UTIL_OUT_DIR should be set to a directory where intermediate output
# will be stored. The output designated for usage by tests will be copied to the
# current directory, so $CA_CERT_UTIL_OUT_DIR can be deleted after all
# certificates have been issued.  However, it should not be tampered with
# between calls to the functions, because e.g. private keys will be stored
# there which may be reused by subsequent calls to issue certificates.
# This script supports creating certificates with 2048 bit RSA keys, as well as
# certificates with EC keys using the P-256 curve.


# Tries to perform an operation. If it fails, log the exit code and exit the
# script with the same exit code.
try() {
  "$@" || {
    e=$?
    echo "*** ERROR $e ***  $@  " > /dev/stderr
    exit $e
  }
}

# Create a self-signed CA cert with CommonName $CN and store it at $1.pem.
# Will write intermediate output to $CA_CERT_UTIL_OUT_DIR. Do not delete
# $CA_CERT_UTIL_OUT_DIR before all certificates have been issued.
# Uses 2048 bit RSA keys.
root_cert_rsa() {
  cert_name="$1"

  try /bin/sh -c "echo 01 > \"${CA_CERT_UTIL_OUT_DIR}/${cert_name}-serial\""
  try touch "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-index.txt"
  try openssl genrsa -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" 2048

  CA_CNF=$(dirname "$0")
  CA_ID=$1 \
    try openssl req \
      -new \
      -key "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
      -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
      -config "${CA_CERT_UTIL_DIR}/ca_rsa.cnf"

  CA_ID=$1 \
    try openssl x509 \
      -req -days 3650 \
      -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
      -signkey "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
      -extfile "${CA_CERT_UTIL_DIR}/ca_rsa.cnf" \
      -extensions ca_cert > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem"

  try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" "${cert_name}.pem"
}

# Create a self-signed CA cert with CommonName $CN and store it at $1.pem.
# Will write intermediate output to $CA_CERT_UTIL_OUT_DIR. Do not delete
# $CA_CERT_UTIL_OUT_DIR before all certificates have been issued.
# Uses EC keys with the P-256 curve.
root_cert_ec() {
  cert_name="$1"

  try /bin/sh -c "echo 01 > \"${CA_CERT_UTIL_OUT_DIR}/${cert_name}-serial\""
  try touch "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-index.txt"
  try openssl ecparam \
  -name prime256v1 \
  -genkey \
  -noout \
  -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key"

  CA_CNF=$(dirname "$0")
  CA_ID=$1 \
    try openssl req \
      -new \
      -key "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
      -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
      -config "${CA_CERT_UTIL_DIR}/ca_ec.cnf"

  CA_ID=$1 \
    try openssl x509 \
      -req -days 3650 \
      -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
      -signkey "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
      -extfile "${CA_CERT_UTIL_DIR}/ca_ec.cnf" \
      -extensions ca_cert > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem"

  try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" "${cert_name}.pem"
}

# Create a cert with CommonName $CN signed by $CA_ID.
# Usage:
#   CA_ID=<id> CN="<cn>" [SAN="<san>"] \
#     issue_cert <$1=file_name> <$2=cert_type> [as_pem] [as_der] [as_pk8]
# and store it at $1.der /$1.pem.
# For more information about specifying Subject Alternative Name (SAN) please
# refer to: https://www.openssl.org/docs/man1.1.1/man5/x509v3_config.html
# $1 / file_name is the name (without extension) of the created files.
# $2 / cert_type must one of:
#  (*) "leaf_cert_san" (for a server/user cert that uses subjectAltName $SAN)
#  (*) "leaf_cert_without_san" (for a server/user cert that does not have a
#      subjectAltName)
#  (*) or "ca_cert" (for a intermediate CA).
# Note that the config for these types is in the file ca{_ec}.cnf.
# The additional parameters specify which output formats will be used.
#  (*) If "as_pem" is passed, a $1.pem file will be created in the current
#      directory.
#  (*) If "as_der" is passed, a $1.der file will be created in the current
#      directory.
#  (*) If "as_pk8" is passed, a $1.pk8 file will be created in the current
#      directory.
# Will write intermediate output to $CA_CERT_UTIL_OUT_DIR. Do not delete
# $CA_CERT_UTIL_OUT_DIR before all certificates have been issued.

# Examples:

# CN=root_ca_cert \
#  try root_cert root_ca_cert

# CA_ID=root_ca_cert CN="127.0.0.1" \
#  try issue_cert ok_cert_without_san leaf_cert_without_san as_pem

# CA_ID=root_ca_cert CN="127.0.0.1" SAN="DNS:example.com" \
#  try issue_cert ok_cert_with_dns_san leaf_cert_san as_pem

# CA_ID=root_ca_cert CN="127.0.0.1" SAN="email:example@domain.com" \
#  try issue_cert ok_cert_with_email_san leaf_cert_san as_pem

# CA_ID=root_ca_cert CN="127.0.0.1" SAN="DNS:www.example.com,IP:192.168.7.1" \
#  try issue_cert ok_cert_with_multiple_sans leaf_cert_san as_pem

# CA_ID=root_ca_cert CN="127.0.0.1" \
#  SAN="DNS:www.example.com,DNS:mail.example.com" \
#  try issue_cert ok_cert_with_same_type_sans leaf_cert_san as_pem

issue_cert_rsa() {
  cert_name="$1"
  cert_type="$2"
  config="${CA_CERT_UTIL_DIR}/ca_rsa.cnf"

  if [[ "${cert_type}" == "ca_cert" ]]
  then
    try /bin/sh -c "echo 01 > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-serial""
    try touch "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-index.txt"
    try openssl genrsa -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" 2048
  fi

  case "${cert_type}" in
    "leaf_cert_san")
      config="${CA_CERT_UTIL_DIR}/cert_with_san_rsa.cnf"
      ;;
    "leaf_cert_without_san")
      config="${CA_CERT_UTIL_DIR}/cert_without_san_rsa.cnf"
      ;;
  esac

  try openssl req \
    -new \
    -keyout "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -config $config

  try openssl ca \
    -batch \
    -extensions "${cert_type}" \
    -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" \
    -config $config

  try openssl x509 -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" -outform DER \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.der"

  try openssl pkcs8 -topk8 -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
    -out ${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pk8 -outform DER -nocrypt

  # Eat the first two arguments to check the output types
  shift
  shift

  # Evaluate requested output formats
  while (( "$#" )); do
    case $1 in
      as_pem)
        echo "Writing ${cert_name}.pem"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" "${cert_name}.pem"
        ;;
      as_der)
        echo "Writing ${cert_name}.der"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.der" "${cert_name}.der"
        ;;
      as_pk8)
        echo "Writing ${cert_name}.pk8"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pk8" "${cert_name}.pk8"
        ;;
    esac

    shift
  done
}

issue_cert_ec() {
  cert_name="$1"
  cert_type="$2"
  config="${CA_CERT_UTIL_DIR}/ca_ec.cnf"

  # In case of EC keys, we always generate the private key here as `openssl req`
  # with the -newkey argument seems to always generate RSA keys.
  try /bin/sh -c "echo 01 > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-serial""
  try touch "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-index.txt"
  try openssl ecparam \
  -name prime256v1 \
  -genkey \
  -noout \
  -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key"

  case "${cert_type}" in
    "leaf_cert_san")
      config="${CA_CERT_UTIL_DIR}/cert_with_san_ec.cnf"
      ;;
    "leaf_cert_without_san")
      config="${CA_CERT_UTIL_DIR}/cert_without_san_ec.cnf"
      ;;
  esac

  try openssl req \
    -new \
    -key "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -config $config

  try openssl ca \
    -batch \
    -extensions "${cert_type}" \
    -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" \
    -config $config

  try openssl x509 -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" -outform DER \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.der"

  try openssl pkcs8 -topk8 -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
    -out ${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pk8 -outform DER -nocrypt

  # Eat the first two arguments to check the output types
  shift
  shift

  # Evaluate requested output formats
  while (( "$#" )); do
    case $1 in
      as_pem)
        echo "Writing ${cert_name}.pem"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" "${cert_name}.pem"
        ;;
      as_der)
        echo "Writing ${cert_name}.der"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.der" "${cert_name}.der"
        ;;
      as_pk8)
        echo "Writing ${cert_name}.pk8"
        try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pk8" "${cert_name}.pk8"
        ;;
    esac

    shift
  done
}

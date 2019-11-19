# Copyright 2019 The Chromium Authors. All rights reserved.
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
# All certificates issued by functions in this script currently use 2048 bit RSA
# keys.


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
root_cert() {
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
      -config "${CA_CERT_UTIL_DIR}/ca.cnf"

  CA_ID=$1 \
    try openssl x509 \
      -req -days 3650 \
      -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
      -signkey "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
      -extfile "${CA_CERT_UTIL_DIR}/ca.cnf" \
      -extensions ca_cert > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem"

  try cp "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" "${cert_name}.pem"
}

# Create a cert with CommonName $CN signed by $CA_ID.
# Usage:
#   CA_ID=<id> CN="<cn>" \
#     issue_cert <$1=file_name> <$2=cert_type> [as_pem] [as_der] [as_pk8]
# and store it at $1.der /$1.pem.
# $1 / file_name is the name (without extension) of the created files.
# $2 / cert_type must one of:
#  (*) "leaf_cert_san_ip" (for a server/user cert that reuses the $CN as
#      subjectAltName of type IP)
#  (*) "leaf_cert_san_dns" (for a server/user cert that reuses the $CN as
#      subjectAltName of type DNS)
#  (*) "leaf_cert_without_san" (for a server/user cert that does not have a
#      subjectAltName)
#  (*) or "ca_cert" (for a intermediate CA).
# Note that the config for these types is in the file ca.cnf.
# The additional parameters specify which output formats will be used.
#  (*) If "as_pem" is passed, a $1.pem file will be created in the current
#      directory.
#  (*) If "as_der" is passed, a $1.der file will be created in the current
#      directory.
#  (*) If "as_pk8" is passed, a $1.pk8 file will be created in the current
#      directory.
# Will write intermediate output to $CA_CERT_UTIL_OUT_DIR. Do not delete
# $CA_CERT_UTIL_OUT_DIR before all certificates have been issued.
issue_cert() {
  cert_name="$1"
  cert_type="$2"

  if [[ "${cert_type}" == "ca_cert" ]]
  then
    try /bin/sh -c "echo 01 > "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-serial""
    try touch "${CA_CERT_UTIL_OUT_DIR}/${cert_name}-index.txt"
    try openssl genrsa -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" 2048
  fi
  try openssl req \
    -new \
    -keyout "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.key" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -config "${CA_CERT_UTIL_DIR}/ca.cnf"

  try openssl ca \
    -batch \
    -extensions "${cert_type}" \
    -in "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.req" \
    -out "${CA_CERT_UTIL_OUT_DIR}/${cert_name}.pem" \
    -config "${CA_CERT_UTIL_DIR}/ca.cnf"

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

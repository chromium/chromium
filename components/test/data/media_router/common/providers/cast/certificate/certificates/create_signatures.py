#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Helper code to generate SHA1 and SHA256 signatures given a private key.
Expects CWD to be the scripts directory.
"""

import base64
import os
import subprocess
import sys
sys.path += ['../../../../../net/data/verify_certificate_chain_unittest']

import common


def sign_data(key_path, data_to_sign, digest):
  """Returns the signature of |data_to_sign| using the key at |key_path| and
  the digest algorithm |digest|. The |digest| parameter should be either
  "sha256" or "sha1"""

  data_to_sign_path = 'out/tmp_data_to_sign'
  signed_data_path = 'out/tmp_signed_data'

  common.write_string_to_file(data_to_sign, data_to_sign_path)

  subprocess.check_call(['openssl', 'dgst', '-' + digest,
                         '-sign', key_path,
                         '-out', signed_data_path,
                         data_to_sign_path ])

  signature = common.read_file_to_string(signed_data_path)

  # Delete the temporary files.
  os.remove(data_to_sign_path)
  os.remove(signed_data_path)

  return signature


def create_signed_data(key_path, signed_data_pem_path, cert_path):
  # Use some random data as the message.
  data_to_sign = os.urandom(256)

  sha1_signature = sign_data(key_path, data_to_sign, 'sha1')
  sha256_signature = sign_data(key_path, data_to_sign, 'sha256')

  # Write a final PEM file which incorporates the message, and signatures.
  signed_data_pem_data = """
These signatures were generated using the device certificate key from:
  %s

The data being signed is a bunch of random data.

-----BEGIN MESSAGE-----
%s
-----END MESSAGE-----

Signature Algorithm: RSASSA PKCS#1 v1.5 with SHA1

-----BEGIN SIGNATURE SHA1-----
%s
-----END SIGNATURE SHA1-----

Signature Algorithm: RSASSA PKCS#1 v1.5 with SHA256

-----BEGIN SIGNATURE SHA256-----
%s
-----END SIGNATURE SHA256----- """ % (cert_path,
       base64.b64encode(data_to_sign),
       base64.b64encode(sha1_signature),
       base64.b64encode(sha256_signature))

  common.write_string_to_file(signed_data_pem_data, signed_data_pem_path)

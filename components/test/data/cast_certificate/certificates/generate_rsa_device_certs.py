#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This python script generates test certificate chains for RSA device certs of
varying strengths (1024 and 2048 bits).

Must be run from the current directory.
"""

import sys
sys.path += ['../../../../../net/data/verify_certificate_chain_unittest']

import common
import create_signatures


def generate_rsa_cert(leaf_key_size):
  JAN_2015 = '150101120000Z'
  JAN_2018 = '180101120000Z'

  # Self-signed root certificate.
  root = common.create_self_signed_root_certificate('Root')
  root.set_validity_range(JAN_2015, JAN_2018)

  # Intermediate certificate.
  intermediate = common.create_intermediate_certificate('Intermediate', root)
  intermediate.set_validity_range(JAN_2015, JAN_2018)

  # Leaf certificate.
  leaf = common.create_end_entity_certificate(
      'RSA %d Device Cert' % leaf_key_size, intermediate)
  leaf.get_extensions().set_property('extendedKeyUsage', 'clientAuth')
  device_key_path = common.create_key_path(leaf.name)
  leaf.set_key(common.get_or_generate_rsa_key(leaf_key_size, device_key_path))
  leaf.set_validity_range(JAN_2015, JAN_2018)

  chain = [leaf, intermediate, root]
  chain_description = """Cast certificate chain where device certificate uses a
  %d-bit RSA key""" % leaf_key_size

  # Write the certificate chain.
  chain_path ='rsa%d_device_cert.pem' % leaf_key_size
  common.write_chain(chain_description, chain, chain_path)

  # Write the the signed data file.
  create_signatures.create_signed_data(
      device_key_path,
      '../signeddata/rsa%d_device_cert_data.pem' % leaf_key_size,
      '../certificates/' + chain_path)


generate_rsa_cert(1024)
generate_rsa_cert(2048)

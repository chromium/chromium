#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This python script generates a number of test certificate chains for policies
(in particular the Audio Only policy). The resulting files have the name
format:

    policies_ica_%s_leaf_%s.pem

Must be run from the current directory.
"""

import sys
sys.path += ['../../../../../net/data/verify_certificate_chain_unittest']

import common


# OID for Cast's "Audio Only" policy.
AUDIO_ONLY = 'audioOnly'

# Symbolic OID for anyPolicy (2.5.29.32.0).
ANY_POLICY = 'anyPolicy'

# Random unknown OID (https://davidben.net/oid), used as unrecognized policy.
FOO = 'foo'

POLICY_SYMBOL_TO_OID = {}
POLICY_SYMBOL_TO_OID[AUDIO_ONLY] = '1.3.6.1.4.1.11129.2.5.2'
POLICY_SYMBOL_TO_OID[ANY_POLICY] = '2.5.29.32.0'
POLICY_SYMBOL_TO_OID[FOO] = '1.2.840.113554.4.1.72585.2'


def set_policies_from_list(certificate, policies):
  if len(policies) == 0:
    certificate.get_extensions().remove_property('certificatePolicies')
    return

  # OpenSSL expects a comma-separate list of OIDs. Translate occurrences of
  # our symbolic values into dotted OIDs.
  policies = [POLICY_SYMBOL_TO_OID.get(x, x) for x in policies]
  certificate.get_extensions().set_property('certificatePolicies',
                                            ','.join(policies))


def policies_to_filename(policies):
  if len(policies) == 0:
    return 'none'
  return ('_'.join(policies)).lower()


JAN_2015 = '150101120000Z'
JAN_2018 = '180101120000Z'

def generate_policies_chain(intermediate_policies, leaf_policies):
  """Creates a certificate chain and writes it to a PEM file (in the current
  directory).

  The chain has 3 certificates (root, intermediate, leaf). The root has no
  policies extension, whereas the intermediate has policies given by
  |intermediate_policies| and the leaf has policies given by |leaf_policies|.

  The policies are specified as a list, with the empty list meaning no policies
  extension. Values in the list should be one of the OID constants (AUDIO_ONLY,
  ANY_POLICY).

  The name of the generated file is a human-readable serialization of this
  function's parameters.
  """

  # Self-signed root certificate.
  root = common.create_self_signed_root_certificate('Root')
  root.set_validity_range(JAN_2015, JAN_2018)

  # Intermediate certificate.
  intermediate = common.create_intermediate_certificate('Intermediate', root)
  set_policies_from_list(intermediate, intermediate_policies)
  intermediate.set_validity_range(JAN_2015, JAN_2018)

  # Leaf certificate.
  leaf = common.create_end_entity_certificate('Leaf', intermediate)
  set_policies_from_list(leaf, leaf_policies)
  leaf.get_extensions().set_property('extendedKeyUsage', 'clientAuth')
  leaf.set_validity_range(JAN_2015, JAN_2018)

  chain = [leaf, intermediate, root]
  chain_description = """Cast certificate chain with the following policies:

  Root:           policies={}
  Intermediate:   policies={%s}
  Leaf:           policies={%s}""" % (', '.join(intermediate_policies),
                                      ', '.join(leaf_policies))

  chain_file_name = 'policies_ica_%s_leaf_%s.pem' % (
      policies_to_filename(intermediate_policies),
      policies_to_filename(leaf_policies))

  common.write_chain(chain_description, chain, chain_file_name)


# -----------------------------------------------------
# Generate a number of permutations for policies.
# -----------------------------------------------------

# audioOnly restricted ICA.
generate_policies_chain([AUDIO_ONLY], [])
generate_policies_chain([AUDIO_ONLY], [AUDIO_ONLY])
generate_policies_chain([AUDIO_ONLY], [ANY_POLICY])
generate_policies_chain([AUDIO_ONLY], [FOO])

# Unrestricted ICA (by ommission).
generate_policies_chain([], [])
generate_policies_chain([], [AUDIO_ONLY])
generate_policies_chain([], [ANY_POLICY])
generate_policies_chain([], [FOO])

# Unrestricted ICA (by anyPolicy).
generate_policies_chain([ANY_POLICY], [])
generate_policies_chain([ANY_POLICY], [AUDIO_ONLY])
generate_policies_chain([ANY_POLICY], [ANY_POLICY])
generate_policies_chain([ANY_POLICY], [FOO])

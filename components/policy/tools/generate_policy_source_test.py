#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import unittest
from unittest.mock import patch, mock_open, call
from typing import NamedTuple

import generate_policy_source
import generate_policy_source_test_data as test_data

from generate_policy_source import PolicyDetails


class PolicyData(NamedTuple):
  policy_id: int
  chunk_number: int
  field_number: int


class PolicyGenerationTest(unittest.TestCase):

  TEMPLATES_JSON = {
      "risk_tag_definitions": [{
          "name": "full-admin-access",
          "description": "full-admin-access-desc",
          "user-description": "full-admin-access-user-desc"
      }],
      "policy_definitions": [{
          "name": "ExampleStringPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:1-"],
          "id": 1,
          "tags": [],
          "caption": "ExampleStringPolicy caption",
          "desc": "ExampleStringPolicy desc"
      }, {
          "name": "ExampleBoolPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-"],
          "id": 2,
          "tags": [],
          "caption": "ExampleBoolPolicy caption",
          "desc": "ExampleBoolPolicy desc",
      }, {
          "name": "ExampleBoolMergeMetapolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-"],
          "features": {
              "metapolicy_type": "merge",
          },
          "id": 3,
          "tags": [],
          "caption": "ExampleBoolMergeMetapolicy caption",
          "desc": "ExampleBoolMergeMetapolicy desc",
      }, {
          "name": "ExampleBoolPrecedenceMetapolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-"],
          "features": {
              "metapolicy_type": "precedence",
          },
          "id": 4,
          "tags": [],
          "caption": "ExampleBoolPrecedenceMetapolicy caption",
          "desc": "ExampleBoolPrecedenceMetapolicy desc",
      }, {
          "name": "CloudOnlyPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "features": {
              "cloud_only": True,
          },
          "supported_on": ["chrome_os:1-", "android:1-"],
          "id": 5,
          "tags": [],
          "caption": "CloudOnlyPolicy caption",
          "desc": "CloudOnlyPolicy desc",
      }, {
          "name": "ChunkZeroLastFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:99-"],
          "id": 979,
          "tags": [],
          "caption": "ChunkZeroLastFieldBooleanPolicy caption",
          "desc": "ChunkZeroLastFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkOneFirstFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:99-"],
          "id": 980,
          "tags": [],
          "caption": "ChunkOneFirstFieldBooleanPolicy caption",
          "desc": "ChunkOneFirstFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkOneLastFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:99-"],
          "id": 1779,
          "tags": [],
          "caption": "ChunkOneLastFieldBooleanPolicy caption",
          "desc": "ChunkOneLastFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkTwoFirstFieldStringPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:99-"],
          "id": 1780,
          "tags": [],
          "caption": "ChunkTwoFirstFieldStringPolicy caption",
          "desc": "ChunkTwoFirstFieldStringPolicy desc"
      }, {
          "name": "ChunkTwoLastFieldStringPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:99-"],
          "id": 2579,
          "tags": [],
          "caption": "ChunkTwoLastFieldStringPolicy caption",
          "desc": "ChunkTwoLastFieldStringPolicy desc"
      }],
      "policy_atomic_group_definitions": []
  }

  def setUp(self):
    self.maxDiff = 10000
    self.chrome_major_version = 94
    self.target_platform = 'chrome_os'
    self.all_target_platforms = ['win', 'mac', 'linux', 'chromeos', 'fuchsia']
    self.risk_tags = generate_policy_source.RiskTags(self.TEMPLATES_JSON)
    self.policies = [
        generate_policy_source.PolicyDetails(policy, self.chrome_major_version,
                                             self.target_platform,
                                             self.risk_tags.GetValidTags())
        for policy in self.TEMPLATES_JSON['policy_definitions']
    ]
    self.risk_tags.ComputeMaxTags(self.policies)

    policy_details_set = list(map((lambda x: x.name), self.policies))
    policies_already_in_group = set()
    self.policy_atomic_groups = [
        generate_policy_source.PolicyAtomicGroup(group, policy_details_set,
                                                 policies_already_in_group)
        for group in self.TEMPLATES_JSON['policy_atomic_group_definitions']
    ]

  def testDefaultValueGeneration(self):
    """Tests generation of default policy values."""
    # Bools
    stmts, expr = generate_policy_source._GenerateDefaultValue(True)
    self.assertListEqual([], stmts)
    self.assertEqual('base::Value(true)', expr)
    stmts, expr = generate_policy_source._GenerateDefaultValue(False)
    self.assertListEqual([], stmts)
    self.assertEqual('base::Value(false)', expr)

    # Ints
    stmts, expr = generate_policy_source._GenerateDefaultValue(33)
    self.assertListEqual([], stmts)
    self.assertEqual('base::Value(33)', expr)

    # Strings
    stmts, expr = generate_policy_source._GenerateDefaultValue('foo')
    self.assertListEqual([], stmts)
    self.assertEqual('base::Value("foo")', expr)

    # Empty list
    stmts, expr = generate_policy_source._GenerateDefaultValue([])
    self.assertListEqual(
        ['base::Value default_value(base::Value::Type::LIST);'], stmts)
    self.assertEqual('std::move(default_value)', expr)

    # List with values
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, '2'])
    self.assertListEqual([
        'base::Value default_value(base::Value::Type::LIST);',
        'default_value.Append(base::Value(1));',
        'default_value.Append(base::Value("2"));'
    ], stmts)
    self.assertEqual('std::move(default_value)', expr)

    # Recursive lists are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, []])
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)

    # Arbitary types are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue(object())
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)

  def _assertCallsEqual(self, call_args_list, expected_output):
    # Convert mocked write calls into actual content that would be written
    # to the file. Elements of call_args_list are call objects, which are
    # two-tuples of (positional args, keyword args). With call[0] we first
    # fetch the positional args, which are an n-tuple, and with call[0][0]
    # we get the first positional argument, which is the string that is
    # written into the file.
    actual_output = ''.join(call[0][0] for call in call_args_list)

    # Strip whitespace from the beginning and end of expected and actual
    # output and verify that they are equal.
    self.assertEqual(actual_output.strip(), expected_output.strip())

  def testWriteCloudPolicyProtobuf(self):
    is_full_runtime_values = [False, True]
    output_path = 'mock_cloud_policy_proto'

    for is_full_runtime in is_full_runtime_values:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WriteCloudPolicyProtobuf(
              self.policies,
              self.policy_atomic_groups,
              self.target_platform,
              f,
              self.risk_tags,
              is_full_runtime=is_full_runtime)

      full_runtime_comment = '//' if is_full_runtime else ''
      full_runtime_suffix = '_full_runtime' if is_full_runtime else ''

      with self.subTest(is_full_runtime=is_full_runtime):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

        expected_formatted = test_data.EXPECTED_CLOUD_POLICY_PROTOBUF % {
            "full_runtime_comment": full_runtime_comment,
            "full_runtime_suffix": full_runtime_suffix,
        }

        self._assertCallsEqual(mocked_file().write.call_args_list,
                               expected_formatted)

  def testWriteChromeSettingsProtobuf(self):
    is_full_runtime_values = [False, True]
    output_path = 'mock_chrome_settings_proto'

    for is_full_runtime in is_full_runtime_values:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WriteChromeSettingsProtobuf(
              self.policies,
              self.policy_atomic_groups,
              self.target_platform,
              f,
              self.risk_tags,
              is_full_runtime=is_full_runtime)

      full_runtime_comment = '//' if is_full_runtime else ''
      full_runtime_suffix = '_full_runtime' if is_full_runtime else ''

      with self.subTest(is_full_runtime=is_full_runtime):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

        expected_formatted = test_data.EXPECTED_CHROME_SETTINGS_PROTOBUF % {
            "full_runtime_comment": full_runtime_comment,
            "full_runtime_suffix": full_runtime_suffix,
        }

        self._assertCallsEqual(mocked_file().write.call_args_list,
                               expected_formatted)

  def testWritePolicyProto(self):
    output_path = 'mock_write_policy_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WritePolicyProto(f, self.policies[0])

    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(mocked_file().write.call_args_list,
                           test_data.EXPECTED_POLICY_PROTO)

  def testGetMetapoliciesOfType(self):
    merge_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "merge")
    self.assertListEqual(["ExampleBoolMergeMetapolicy"], merge_metapolicies)
    self.assertEqual(1, len(merge_metapolicies))

    precedence_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "precedence")
    self.assertListEqual(["ExampleBoolPrecedenceMetapolicy"],
                         precedence_metapolicies)
    self.assertEqual(1, len(precedence_metapolicies))

    invalid_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "invalid")
    self.assertListEqual([], invalid_metapolicies)
    self.assertEqual(0, len(invalid_metapolicies))

  def testWritePolicyConstantHeader(self):
    output_path = 'mock_policy_constants_h'

    for target_platform in self.all_target_platforms:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WritePolicyConstantHeader(
              self.policies,
              self.policy_atomic_groups,
              target_platform,
              f,
              self.risk_tags,
          )
      with self.subTest(target_platform=target_platform):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

        if target_platform == 'win':
          windows_only_part = test_data.POLICY_CONSTANTS_HEADER_WIN_ONLY_PART
        else:
          windows_only_part = ''
        expected_formatted = test_data.EXPECTED_POLICY_CONSTANTS_HEADER % {
            "windows_only_part": windows_only_part,
        }

        self._assertCallsEqual(mocked_file().write.call_args_list,
                               expected_formatted)

  def testWritePolicyConstantSource(self):
    output_path = 'mock_policy_constants_cc'

    for target_platform in self.all_target_platforms:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WritePolicyConstantSource(
              self.policies,
              self.policy_atomic_groups,
              target_platform,
              f,
              self.risk_tags,
          )
      with self.subTest(target_platform=target_platform):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

        if target_platform == 'win':
          windows_only_part = test_data.POLICY_CONSTANTS_SOURCE_WIN_ONLY_PART
        else:
          windows_only_part = ''
        expected_formatted = test_data.EXPECTED_POLICY_CONSTANTS_SOURCE % {
            "windows_only_part": windows_only_part,
        }

        self._assertCallsEqual(mocked_file().write.call_args_list,
                               expected_formatted)

  def testWriteChromeOSPolicyConstantsHeader(self):
    output_path = 'mock_policy_constants_h'
    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteChromeOSPolicyConstantsHeader(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
        )
    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(mocked_file().write.call_args_list,
                           test_data.EXPECTED_CROS_POLICY_CONSTANTS_HEADER)

  def testWriteChromeOSPolicyConstantsSource(self):
    output_path = 'mock_policy_constants_cc'
    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteChromeOSPolicyConstantsSource(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
        )
    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(mocked_file().write.call_args_list,
                           test_data.EXPECTED_CROS_POLICY_CONSTANTS_SOURCE)


  def testWriteAppRestrictions(self):
    output_path = 'app_restrictions_xml'
    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteAppRestrictions(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
        )
    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(mocked_file().write.call_args_list,
                           test_data.EXPECTED_APP_RESTRICTIONS_XML)


  def testChunkNumberAndFieldNumber(self):
    test_data = [
        # Last top-level policy
        PolicyData(policy_id=979, chunk_number=0, field_number=981),
        # First policy in chunk 1
        PolicyData(policy_id=980, chunk_number=1, field_number=1),
        # Last policy in chunk 1
        PolicyData(policy_id=1779, chunk_number=1, field_number=800),
        # First policy in chunk 2
        PolicyData(policy_id=1780, chunk_number=2, field_number=1),
        # Last policy in chunk 2
        PolicyData(policy_id=2579, chunk_number=2, field_number=800),
        # First policy in chunk 3
        PolicyData(policy_id=2580, chunk_number=3, field_number=1),
        # Last policy in chunk 3
        PolicyData(policy_id=3379, chunk_number=3, field_number=800),
        # First policy in chunk 501
        PolicyData(policy_id=400980, chunk_number=501, field_number=1),
        # Last policy in chunk 501
        PolicyData(policy_id=401779, chunk_number=501, field_number=800),
        # First policy in chunk 502
        PolicyData(policy_id=401780, chunk_number=502, field_number=1),
        # Last policy in chunk 502
        PolicyData(policy_id=402579, chunk_number=502, field_number=800),
        # First policy in chunk 503
        PolicyData(policy_id=402580, chunk_number=503, field_number=1),
        # Last policy in chunk 503
        PolicyData(policy_id=403379, chunk_number=503, field_number=800),
    ]

    for policy_data in test_data:
      self.assertEqual(
          generate_policy_source._ChunkNumber(policy_data.policy_id),
          policy_data.chunk_number)
      self.assertEqual(
          generate_policy_source._FieldNumber(policy_data.policy_id,
                                              policy_data.chunk_number),
          policy_data.field_number)


if __name__ == '__main__':
  unittest.main()

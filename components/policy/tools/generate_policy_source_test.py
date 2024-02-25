#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
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
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
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
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 2,
          "tags": [],
          "caption": "ExampleBoolPolicy caption",
          "desc": "ExampleBoolPolicy desc",
      }, {
          "name":
          "ExampleBoolMergeMetapolicy",
          "type":
          "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on":
          ["chrome_os:1-", "chrome.*:1-", "android:1-", "ios:1-", "fuchsia:1-"],
          "features": {
              "metapolicy_type": "merge",
          },
          "id":
          3,
          "tags": [],
          "caption":
          "ExampleBoolMergeMetapolicy caption",
          "desc":
          "ExampleBoolMergeMetapolicy desc",
      }, {
          "name":
          "ExampleBoolPrecedenceMetapolicy",
          "type":
          "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on":
          ["chrome_os:1-", "chrome.*:1-", "android:1-", "ios:1-"],
          "features": {
              "metapolicy_type": "precedence",
          },
          "id":
          4,
          "tags": [],
          "caption":
          "ExampleBoolPrecedenceMetapolicy caption",
          "desc":
          "ExampleBoolPrecedenceMetapolicy desc",
      }, {
          "name":
          "CloudOnlyPolicy",
          "type":
          "main",
          "schema": {
              "type": "boolean"
          },
          "features": {
              "cloud_only": True,
          },
          "supported_on": ["chrome_os:1-", "android:1-", "chrome.*:1-"],
          "id":
          5,
          "tags": [],
          "caption":
          "CloudOnlyPolicy caption",
          "desc":
          "CloudOnlyPolicy desc",
      }, {
          "name":
          "CloudManagementEnrollmentToken",
          "type":
          "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:1-", "android:1-", "chrome.*:1-"],
          "id":
          6,
          "tags": [],
          "caption":
          "CloudManagementEnrollmentToken caption",
          "desc":
          "CloudManagementEnrollmentToken desc"
      }, {
          "name": "DeprecatedNotGenerated",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:1-92"],
          "id": 8,
          "tags": [],
          "caption": "DeprecatedNotGenerated caption",
          "desc": "DeprecatedNotGenerated desc"
      }, {
          "name": "UnsupportedPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": [],
          "id": 9,
          "tags": [],
          "caption": "UnsupportedPolicy caption",
          "desc": "UnsupportedPolicy desc"
      }, {
          "name": "ChunkZeroLastFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 1040,
          "tags": [],
          "caption": "ChunkZeroLastFieldBooleanPolicy caption",
          "desc": "ChunkZeroLastFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkOneFirstFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 1041,
          "tags": [],
          "caption": "ChunkOneFirstFieldBooleanPolicy caption",
          "desc": "ChunkOneFirstFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkOneLastFieldBooleanPolicy",
          "type": "main",
          "schema": {
              "type": "boolean"
          },
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 1840,
          "tags": [],
          "caption": "ChunkOneLastFieldBooleanPolicy caption",
          "desc": "ChunkOneLastFieldBooleanPolicy desc.",
      }, {
          "name": "ChunkTwoFirstFieldStringPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 1841,
          "tags": [],
          "caption": "ChunkTwoFirstFieldStringPolicy caption",
          "desc": "ChunkTwoFirstFieldStringPolicy desc"
      }, {
          "name": "ChunkTwoLastFieldStringPolicy",
          "type": "string",
          "schema": {
              "type": "string"
          },
          "supported_on": ["chrome_os:1-", "chrome.*:1-"],
          "id": 2640,
          "tags": [],
          "caption": "ChunkTwoLastFieldStringPolicy caption",
          "desc": "ChunkTwoLastFieldStringPolicy desc"
      }],
      "policy_atomic_group_definitions": []
  }

  def setUp(self):
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
    self.assertListEqual(['base::Value::List default_value;'], stmts)
    self.assertEqual('base::Value(std::move(default_value))', expr)

    # List with values
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, '2'])
    self.assertListEqual([
        'base::Value::List default_value;',
        'default_value.Append(base::Value(1));',
        'default_value.Append(base::Value("2"));'
    ], stmts)
    self.assertEqual('base::Value(std::move(default_value))', expr)

    # Recursive lists are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, []])
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)

    # Arbitary types are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue(object())
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)

  def _assertCallsEqual(self, expected_output, call_args_list):
    # Convert mocked write calls into actual content that would be written
    # to the file. Elements of call_args_list are call objects, which are
    # two-tuples of (positional args, keyword args). With call[0] we first
    # fetch the positional args, which are an n-tuple, and with call[0][0]
    # we get the first positional argument, which is the string that is
    # written into the file.
    actual_output = ''.join(call[0][0] for call in call_args_list)

    # Strip whitespace from the beginning and end of expected and actual
    # output and verify that they are equal.
    self.assertEqual(expected_output.strip(), actual_output.strip())

  def testWriteCloudPolicyProtobuf(self):
    output_path = 'mock_cloud_policy_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteCloudPolicyProtobuf(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
            chunking=True)

    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

    self._assertCallsEqual(test_data.EXPECTED_CLOUD_POLICY_PROTOBUF,
                           mocked_file().write.call_args_list)

  def testWriteCloudPolicyProtobufNoChunking(self):
    output_path = 'mock_cloud_policy_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteCloudPolicyProtobuf(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
            chunking=False)

    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

    self._assertCallsEqual(test_data.EXPECTED_CLOUD_POLICY_PROTOBUF_NO_CHUNKING,
                           mocked_file().write.call_args_list)

  def testWriteChromeSettingsProtobuf(self):
    output_path = 'mock_chrome_settings_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteChromeSettingsProtobuf(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
            chunking=True)

      mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

      self._assertCallsEqual(test_data.EXPECTED_CHROME_SETTINGS_PROTOBUF,
                             mocked_file().write.call_args_list)

  def testWriteChromeSettingsProtobufNoChunking(self):
    output_path = 'mock_chrome_settings_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WriteChromeSettingsProtobuf(
            self.policies,
            self.policy_atomic_groups,
            self.target_platform,
            f,
            self.risk_tags,
            chunking=False)

      mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')

      self._assertCallsEqual(
          test_data.EXPECTED_CHROME_SETTINGS_PROTOBUF_NO_CHUNKING,
          mocked_file().write.call_args_list)

  def testWritePolicyProto(self):
    output_path = 'mock_write_policy_proto'

    with patch('codecs.open', mock_open()) as mocked_file:
      with codecs.open(output_path, 'w', encoding='utf-8') as f:
        generate_policy_source._WritePolicyProto(f, self.policies[0])

    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(test_data.EXPECTED_POLICY_PROTO,
                           mocked_file().write.call_args_list)

  def testGetMetapoliciesOfType(self):
    merge_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "merge")
    self.assertEqual(1, len(merge_metapolicies))
    self.assertEqual("ExampleBoolMergeMetapolicy", merge_metapolicies[0].name)

    precedence_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "precedence")
    self.assertEqual(1, len(precedence_metapolicies))
    self.assertEqual("ExampleBoolPrecedenceMetapolicy",
                     precedence_metapolicies[0].name)

    invalid_metapolicies = generate_policy_source._GetMetapoliciesOfType(
        self.policies, "invalid")
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
              chunking=True,
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

        self._assertCallsEqual(expected_formatted,
                               mocked_file().write.call_args_list)

  def testWritePolicyConstantSource(self):
    self.maxDiff = None
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
              chunking=True,
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

        self._assertCallsEqual(expected_formatted,
                               mocked_file().write.call_args_list)


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
            chunking=True,
        )
    mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
    self._assertCallsEqual(test_data.EXPECTED_APP_RESTRICTIONS_XML,
                           mocked_file().write.call_args_list)


  def testChunkNumberAndFieldNumber(self):
    test_data = [
        # Last top-level policy
        PolicyData(policy_id=1040, chunk_number=0, field_number=1042),
        # First policy in chunk 1
        PolicyData(policy_id=1041, chunk_number=1, field_number=1),
        # Last policy in chunk 1
        PolicyData(policy_id=1840, chunk_number=1, field_number=800),
        # First policy in chunk 2
        PolicyData(policy_id=1841, chunk_number=2, field_number=1),
        # Last policy in chunk 2
        PolicyData(policy_id=2640, chunk_number=2, field_number=800),
        # First policy in chunk 3
        PolicyData(policy_id=2641, chunk_number=3, field_number=1),
        # Last policy in chunk 3
        PolicyData(policy_id=3440, chunk_number=3, field_number=800),
        # First policy in chunk 501
        PolicyData(policy_id=401041, chunk_number=501, field_number=1),
        # Last policy in chunk 501
        PolicyData(policy_id=401840, chunk_number=501, field_number=800),
        # First policy in chunk 502
        PolicyData(policy_id=401841, chunk_number=502, field_number=1),
        # Last policy in chunk 502
        PolicyData(policy_id=402640, chunk_number=502, field_number=800),
        # First policy in chunk 503
        PolicyData(policy_id=402641, chunk_number=503, field_number=1),
        # Last policy in chunk 503
        PolicyData(policy_id=403440, chunk_number=503, field_number=800),
    ]

    for policy_data in test_data:
      # With chunking:
      self.assertEqual(
          generate_policy_source._ChunkNumber(policy_data.policy_id,
                                              chunking=True),
          policy_data.chunk_number)
      self.assertEqual(
          generate_policy_source._FieldNumber(policy_data.policy_id,
                                              policy_data.chunk_number),
          policy_data.field_number)

      # Without chunking:
      self.assertEqual(
          generate_policy_source._ChunkNumber(policy_data.policy_id,
                                              chunking=False), 0)
      self.assertEqual(
          generate_policy_source._FieldNumber(policy_data.policy_id, 0),
          policy_data.policy_id + 2)


if __name__ == '__main__':
  unittest.main()

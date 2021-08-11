#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import unittest
from unittest.mock import patch, mock_open, call

import generate_policy_source


class CppGenerationTest(unittest.TestCase):

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
          "caption": "caption",
          "tags": [],
          "desc": "desc"
      }],
      "policy_atomic_group_definitions": []
  }

  def setUp(self):
    self.chrome_major_version = 94
    self.target_platform = 'chrome_os'
    self.risk_tags = generate_policy_source.RiskTags(self.TEMPLATES_JSON)
    self.policies = [
        generate_policy_source.PolicyDetails(policy, self.chrome_major_version,
                                             self.target_platform,
                                             self.risk_tags.GetValidTags())
        for policy in self.TEMPLATES_JSON['policy_definitions']
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

  def testWriteCloudPolicyProtobuf(self):
    is_full_runtime_values = [False, True]
    output_path = 'mock_cloud_policy_proto'
    header_write_call = '''
syntax = "proto2";

{}option optimize_for = LITE_RUNTIME;

package enterprise_management;

import "policy_common_definitions{}.proto";
'''

    for is_full_runtime in is_full_runtime_values:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WriteCloudPolicyProtobuf(
              self.policies, [],
              self.target_platform,
              f,
              self.risk_tags,
              is_full_runtime=is_full_runtime)

      full_runtime_comment = '//' if is_full_runtime else ''
      full_runtime_suffix = '_full_runtime' if is_full_runtime else ''

      with self.subTest(is_full_runtime=is_full_runtime):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
        mocked_file().write.assert_has_calls([
            call(
                header_write_call.format(full_runtime_comment,
                                         full_runtime_suffix)),
            call('message CloudPolicySettings {\n'),
            call('  optional StringPolicyProto ExampleStringPolicy = 3;\n'),
            call('}\n\n'),
        ])

  def testWriteChromeSettingsProtobuf(self):
    is_full_runtime_values = [False, True]
    output_path = 'mock_chrome_settings_proto'
    header_write_call = '''
syntax = "proto2";

{}option optimize_for = LITE_RUNTIME;

package enterprise_management;

// For StringList and PolicyOptions.
import "policy_common_definitions{}.proto";

'''

    for is_full_runtime in is_full_runtime_values:
      with patch('codecs.open', mock_open()) as mocked_file:
        with codecs.open(output_path, 'w', encoding='utf-8') as f:
          generate_policy_source._WriteChromeSettingsProtobuf(
              self.policies, [],
              self.target_platform,
              f,
              self.risk_tags,
              is_full_runtime=is_full_runtime)

      full_runtime_comment = '//' if is_full_runtime else ''
      full_runtime_suffix = '_full_runtime' if is_full_runtime else ''

      with self.subTest(is_full_runtime=is_full_runtime):
        mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
        mocked_file().write.assert_has_calls([
            call(
                header_write_call.format(full_runtime_comment,
                                         full_runtime_suffix)),
            call('// PBs for individual settings.\n\n'),
            call('// caption'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// desc'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// Supported on: chrome_os'),
            call('\n'),
            call('message ExampleStringPolicyProto {\n'),
            call('  optional PolicyOptions policy_options = 1;\n'),
            call('  optional string ExampleStringPolicy = 2;\n'),
            call('}\n\n'),
            call('''// --------------------------------------------------
// Big wrapper PB containing the above groups.

message ChromeSettingsProto {
'''),
            call(
                '  optional ExampleStringPolicyProto ExampleStringPolicy = 3;\n'
            ),
            call('}\n\n'),
        ])


if __name__ == '__main__':
  unittest.main()

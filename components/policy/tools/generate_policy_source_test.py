#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import unittest
from unittest.mock import patch, mock_open, call

import generate_policy_source

from generate_policy_source import PolicyDetails


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
        mocked_file().write.assert_has_calls([
            call(
                header_write_call.format(full_runtime_comment,
                                         full_runtime_suffix)),
            call('message CloudPolicySettings {\n'),
            call('  optional StringPolicyProto ExampleStringPolicy = 3;\n'),
            call('  optional BooleanPolicyProto ExampleBoolPolicy = 4;\n'),
            call('  optional BooleanPolicyProto '
                 'ExampleBoolMergeMetapolicy = 5;\n'),
            call('  optional BooleanPolicyProto '
                 'ExampleBoolPrecedenceMetapolicy = 6;\n'),
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
        mocked_file().write.assert_has_calls([
            call(
                header_write_call.format(full_runtime_comment,
                                         full_runtime_suffix)),
            call('// PBs for individual settings.\n\n'),
            call('// ExampleStringPolicy caption'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// ExampleStringPolicy desc'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// Supported on: chrome_os'),
            call('\n'),
            call('message ExampleStringPolicyProto {\n'),
            call('  optional PolicyOptions policy_options = 1;\n'),
            call('  optional string ExampleStringPolicy = 2;\n'),
            call('}\n\n'),
            call('// ExampleBoolPolicy caption'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// ExampleBoolPolicy desc'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// Supported on: chrome_os'),
            call('\n'),
            call('message ExampleBoolPolicyProto {\n'),
            call('  optional PolicyOptions policy_options = 1;\n'),
            call('  optional bool ExampleBoolPolicy = 2;\n'),
            call('}\n\n'),
            call('// ExampleBoolMergeMetapolicy caption'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// ExampleBoolMergeMetapolicy desc'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// Supported on: chrome_os'),
            call('\n'),
            call('message ExampleBoolMergeMetapolicyProto {\n'),
            call('  optional PolicyOptions policy_options = 1;\n'),
            call('  optional bool ExampleBoolMergeMetapolicy = 2;\n'),
            call('}\n\n'),
            call('// ExampleBoolPrecedenceMetapolicy caption'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// ExampleBoolPrecedenceMetapolicy desc'),
            call('\n'),
            call('//'),
            call('\n'),
            call('// Supported on: chrome_os'),
            call('\n'),
            call('message ExampleBoolPrecedenceMetapolicyProto {\n'),
            call('  optional PolicyOptions policy_options = 1;\n'),
            call('  optional bool ExampleBoolPrecedenceMetapolicy = 2;\n'),
            call('}\n\n'),
            call('''// --------------------------------------------------
// Big wrapper PB containing the above groups.

message ChromeSettingsProto {
'''),
            call(
                '  optional ExampleStringPolicyProto ExampleStringPolicy = 3;\n'
                '  optional ExampleBoolPolicyProto ExampleBoolPolicy = 4;\n'
                '  optional ExampleBoolMergeMetapolicyProto '
                'ExampleBoolMergeMetapolicy = 5;\n'
                '  optional ExampleBoolPrecedenceMetapolicyProto '
                'ExampleBoolPrecedenceMetapolicy = 6;\n'),
            call('}\n\n'),
        ])

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
    expected_file_calls_default_first_part = [
        call('''\
#ifndef COMPONENTS_POLICY_POLICY_CONSTANTS_H_
#define COMPONENTS_POLICY_POLICY_CONSTANTS_H_

#include <cstdint>
#include <string>

#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace policy {

namespace internal {
struct SchemaData;
}

''')
    ]
    expected_file_calls_default_win_part = [
        call('''\
// The windows registry path where Chrome policy configuration resides.
extern const wchar_t kRegistryChromePolicyKey[];
''')
    ]
    expected_file_calls_default_second_part = [
        call('''\
#if defined(OS_CHROMEOS)
// Sets default profile policies values for enterprise users.
void SetEnterpriseUsersProfileDefaults(PolicyMap* policy_map);
// Sets default system-wide policies values for enterprise users.
void SetEnterpriseUsersSystemWideDefaults(PolicyMap* policy_map);
// Sets all default values for enterprise users.
void SetEnterpriseUsersDefaults(PolicyMap* policy_map);
#endif

// Returns the PolicyDetails for |policy| if |policy| is a known
// Chrome policy, otherwise returns nullptr.
const PolicyDetails* GetChromePolicyDetails(
const std::string& policy);

// Returns the schema data of the Chrome policy schema.
const internal::SchemaData* GetChromeSchemaData();

'''),
        call('// Key names for the policy settings.\nnamespace key {\n\n'),
        call('extern const char kExampleStringPolicy[];\n'),
        call('extern const char kExampleBoolPolicy[];\n'),
        call('extern const char kExampleBoolMergeMetapolicy[];\n'),
        call('extern const char kExampleBoolPrecedenceMetapolicy[];\n'),
        call('\n}  // namespace key\n\n'),
        call('// Group names for the policy settings.\nnamespace group {\n\n'),
        call('\n}  // namespace group\n\n'),
        call('struct AtomicGroup {\n'
             '  const short id;\n'
             '  const char* policy_group;\n'
             '  const char* const* policies;\n'
             '};\n\n'),
        call('extern const AtomicGroup kPolicyAtomicGroupMappings[];\n\n'),
        call('extern const size_t kPolicyAtomicGroupMappingsLength;\n\n'),
        call('// Arrays of metapolicies.\nnamespace metapolicy {\n\n'),
        call('extern const char* kMerge[1];\n'),
        call('extern const char* kPrecedence[1];\n\n'),
        call('}  // namespace metapolicy\n\n'),
        call('enum class StringPolicyType {\n'
             '  STRING,\n  JSON,\n  EXTERNAL,\n'
             '};\n\n'),
        call('''\
// Read access to the protobufs of all supported boolean user policies.
'''),
        call('struct BooleanPolicyAccess {\n'),
        call('''\
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::BooleanPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
'''),
        call('};\n'),
        call('extern const BooleanPolicyAccess kBooleanPolicyAccess[];\n\n'),
        call('''\
// Read access to the protobufs of all supported integer user policies.
'''),
        call('struct IntegerPolicyAccess {\n'),
        call('''\
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::IntegerPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
'''),
        call('};\n'),
        call('extern const IntegerPolicyAccess kIntegerPolicyAccess[];\n\n'),
        call('''\
// Read access to the protobufs of all supported string user policies.
'''),
        call('struct StringPolicyAccess {\n'),
        call('''\
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::StringPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
'''),
        call('  const StringPolicyType type;\n'),
        call('};\n'),
        call('extern const StringPolicyAccess kStringPolicyAccess[];\n\n'),
        call('''\
// Read access to the protobufs of all supported stringlist user policies.
'''),
        call('struct StringListPolicyAccess {\n'),
        call('''\
  const char* policy_key;
  bool per_profile;
  bool (enterprise_management::CloudPolicySettings::*has_proto)() const;
  const enterprise_management::StringListPolicyProto&
      (enterprise_management::CloudPolicySettings::*get_proto)() const;
'''),
        call('};\n'),
        call('extern const StringListPolicyAccess '
             'kStringListPolicyAccess[];\n\n'),
        call('constexpr int64_t '
             'kDevicePolicyExternalDataResourceCacheSize = 0;\n'),
        call('''\

}  // namespace policy

#endif  // COMPONENTS_POLICY_POLICY_CONSTANTS_H_
''')
    ]

    expected_file_calls_default = (expected_file_calls_default_first_part +
                                   expected_file_calls_default_second_part)
    # Win header has special lines after 'struct SchemaData;' declaration.
    expected_file_calls_win = (expected_file_calls_default_first_part +
                               expected_file_calls_default_win_part +
                               expected_file_calls_default_second_part)

    expected_file_calls = {
        platform: expected_file_calls_default
        for platform in self.all_target_platforms
    }
    expected_file_calls['win'] = expected_file_calls_win

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
        mocked_file().write.assert_has_calls(
            expected_file_calls[target_platform])

  def testWritePolicyConstantSource(self):
    output_path = 'mock_policy_constants_cc'

    expected_file_calls_default_first_part = [
        call('''\
#include "components/policy/policy_constants.h"

#include <algorithm>
#include <climits>
#include <memory>

#include "base/check_op.h"
#include "base/stl_util.h"  // base::size()
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_internal.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/risk_tag.h"

namespace em = enterprise_management;

namespace policy {

'''),
        call('''\
const __attribute__((unused)) PolicyDetails kChromePolicyDetails[] = {
// is_deprecated is_future is_device_policy id max_external_data_size, risk tags
'''),
        call('  // ExampleStringPolicy\n'),
        # No actual new lines below, just a split of long line.
        call('  { false,        false,    false,'
             '              1,                     0, {  } },\n'),
        call('  // ExampleBoolPolicy\n'),
        call('  { false,        false,    false,'
             '              2,                     0, {  } },\n'),
        call('  // ExampleBoolMergeMetapolicy\n'),
        call('  { false,        false,    false,'
             '              3,                     0, {  } },\n'),
        call('  // ExampleBoolPrecedenceMetapolicy\n'),
        call('  { false,        false,    false,'
             '              4,                     0, {  } },\n'),
        call('};\n\n'),
        call('''\
const internal::SchemaNode kSchemas[] = {
//  Type                           Extra  IsSensitiveValue HasSensitiveChildren
'''),
        # No actual new lines below, just a split of long line.
        call('  { base::Value::Type::DICTIONARY,     '
             '0, false,           false },  // root node\n'),
        call('  { base::Value::Type::BOOLEAN,       '
             '-1, false,           false },  // simple type: boolean\n'),
        call('  { base::Value::Type::STRING,        '
             '-1, false,           false },  // simple type: string\n'),
        call('};\n\n'),
        call('''\
const internal::PropertyNode kPropertyNodes[] = {
//  Property                                                             Schema
'''),
        # No actual new lines below, just a split of long line.
        call('  { key::kExampleBoolMergeMetapolicy,'
             '                                     1 },\n'),
        call('  { key::kExampleBoolPolicy,'
             '                                              1 },\n'),
        call('  { key::kExampleBoolPrecedenceMetapolicy,'
             '                                1 },\n'),
        call('  { key::kExampleStringPolicy,'
             '                                            2 },\n'),
        call('};\n\n'),
        call('''\
const internal::PropertiesNode kProperties[] = {
//  Begin    End  PatternEnd  RequiredBegin  RequiredEnd  Additional Properties
'''),
        # No actual new lines below, just a split of long line.
        call('  {     0,     4,     4,'
             '     0,          0,    -1 },  // root node\n'),
        call('};\n\n'),
        call('const internal::SchemaData* GetChromeSchemaData() {\n'),
        call('''\
  static const internal::SchemaData kChromeSchemaData = {
    kSchemas,
'''),
        call('    kPropertyNodes,\n'),
        call('    kProperties,\n'),
        call('  nullptr,\n'),
        call('  nullptr,\n'),
        call('  nullptr,\n'),
        call('  nullptr,\n'),
        call('    -1,  // validation_schema root index\n'),
        call('  };\n\n'),
        call('  return &kChromeSchemaData;\n}\n\n'),
        call('\n'),
        call('namespace {\n'),
        call('''\
bool CompareKeys(const internal::PropertyNode& node,
                 const std::string& key) {
  return node.key < key;
}

'''),
        call('}  // namespace\n\n')
    ]

    expected_file_calls_default_win_part = [
        call('''\
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kRegistryChromePolicyKey[] = \
L"SOFTWARE\\\\Policies\\\\Google\\\\Chrome";
#else
const wchar_t kRegistryChromePolicyKey[] = L"SOFTWARE\\\\Policies\\\\Chromium";
#endif

''')
    ]

    expected_file_calls_default_second_part = [
        call('#if defined(OS_CHROMEOS)'),
        # Note no \ and new lines in three calls below.
        call('''
void SetEnterpriseUsersProfileDefaults(PolicyMap* policy_map) {

}
'''),
        call('''
void SetEnterpriseUsersSystemWideDefaults(PolicyMap* policy_map) {

}
'''),
        call('''
void SetEnterpriseUsersDefaults(PolicyMap* policy_map) {
  SetEnterpriseUsersProfileDefaults(policy_map);
  SetEnterpriseUsersSystemWideDefaults(policy_map);
}
'''),
        call('#endif\n\n'),
        call('''\
const PolicyDetails* GetChromePolicyDetails(const std::string& policy) {
'''),
        call('''\
  // First index in kPropertyNodes of the Chrome policies.
  static const int begin_index = 0;
  // One-past-the-end of the Chrome policies in kPropertyNodes.
  static const int end_index = 4;
'''),
        call("""\
  const internal::PropertyNode* begin =
     kPropertyNodes + begin_index;
  const internal::PropertyNode* end = kPropertyNodes + end_index;
  const internal::PropertyNode* it =
      std::lower_bound(begin, end, policy, CompareKeys);
  if (it == end || it->key != policy)
    return nullptr;
  // This relies on kPropertyNodes from begin_index to end_index
  // having exactly the same policies (and in the same order) as
  // kChromePolicyDetails, so that binary searching on the first
  // gets the same results as a binary search on the second would.
  // However, kPropertyNodes has the policy names and
  // kChromePolicyDetails doesn't, so we obtain the index into
  // the second array by searching the first to avoid duplicating
  // the policy name pointers.
  // Offsetting |it| from |begin| here obtains the index we're
  // looking for.
  size_t index = it - begin;
  CHECK_LT(index, base::size(kChromePolicyDetails));
  return kChromePolicyDetails + index;
"""),
        call('}\n\n'),
        call('namespace key {\n\n'),
        call('const char kExampleStringPolicy[] = "ExampleStringPolicy";\n'),
        call('const char kExampleBoolPolicy[] = "ExampleBoolPolicy";\n'),
        call('const char kExampleBoolMergeMetapolicy[] = '
             '"ExampleBoolMergeMetapolicy";\n'),
        call('const char kExampleBoolPrecedenceMetapolicy[] = '
             '"ExampleBoolPrecedenceMetapolicy";\n'),
        call('\n}  // namespace key\n\n'),
        call('namespace group {\n\n'),
        call('\n'),
        call('namespace {\n\n'),
        call('\n}  // namespace\n'),
        call('\n}  // namespace group\n\n'),
        call('const AtomicGroup kPolicyAtomicGroupMappings[] = {\n'),
        call('};\n\n'),
        call('const size_t kPolicyAtomicGroupMappingsLength = 0;\n\n'),
        call('namespace metapolicy {\n\n'),
        call('const char* kMerge[1] = {\n'),
        call('  key::kExampleBoolMergeMetapolicy,\n'),
        call('};\n\n'),
        call('const char* kPrecedence[1] = {\n'),
        call('  key::kExampleBoolPrecedenceMetapolicy,\n'),
        call('};\n\n'),
        call('}  // namespace metapolicy\n\n'),
        call('const BooleanPolicyAccess kBooleanPolicyAccess[] = {\n'),
        call('''\
  {key::kExampleBoolPolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolpolicy,
   &em::CloudPolicySettings::exampleboolpolicy},
'''),
        call('''\
  {key::kExampleBoolMergeMetapolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolmergemetapolicy,
   &em::CloudPolicySettings::exampleboolmergemetapolicy},
'''),
        call('''\
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   &em::CloudPolicySettings::has_exampleboolprecedencemetapolicy,
   &em::CloudPolicySettings::exampleboolprecedencemetapolicy},
'''),
        call('  {nullptr, false, nullptr, nullptr},\n};\n\n'),
        call('const IntegerPolicyAccess kIntegerPolicyAccess[] = {\n'),
        call('  {nullptr, false, nullptr, nullptr},\n};\n\n'),
        call('const StringPolicyAccess kStringPolicyAccess[] = {\n'),
        call('''\
  {key::kExampleStringPolicy,
   false,
   &em::CloudPolicySettings::has_examplestringpolicy,
   &em::CloudPolicySettings::examplestringpolicy,
   StringPolicyType::STRING},
'''),
        call('  {nullptr, false, nullptr, nullptr},\n};\n\n'),
        call('const StringListPolicyAccess kStringListPolicyAccess[] = {\n'),
        call('  {nullptr, false, nullptr, nullptr},\n};\n\n'),
        call('\n}  // namespace policy\n')
    ]

    expected_file_calls_default = (expected_file_calls_default_first_part +
                                   expected_file_calls_default_second_part)
    # Win source has special lines after 'CompareKeys' implementations.
    expected_file_calls_win = (expected_file_calls_default_first_part +
                               expected_file_calls_default_win_part +
                               expected_file_calls_default_second_part)

    expected_file_calls = {
        platform: expected_file_calls_default
        for platform in self.all_target_platforms
    }
    expected_file_calls['win'] = expected_file_calls_win

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
        mocked_file().write.assert_has_calls(
            expected_file_calls[target_platform])

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
    with self.subTest():
      mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
      mocked_file().write.assert_has_calls([
          call('''\
#ifndef __BINDINGS_POLICY_CONSTANTS_H_
#define __BINDINGS_POLICY_CONSTANTS_H_

'''),
          call('namespace enterprise_management {\n'
               'class CloudPolicySettings;\n'),
          call('class BooleanPolicyProto;\n'),
          call('class IntegerPolicyProto;\n'),
          call('class StringPolicyProto;\n'),
          call('class StringListPolicyProto;\n'),
          call('}  // namespace enterprise_management\n\n'),
          call('namespace policy {\n\n'),
          call('''\
// Registry key names for user and device policies.
namespace key {

'''),
          call('extern const char kExampleStringPolicy[];\n'),
          call('extern const char kExampleBoolPolicy[];\n'),
          call('extern const char kExampleBoolMergeMetapolicy[];\n'),
          call('extern const char kExampleBoolPrecedenceMetapolicy[];\n'),
          call('\n}  // namespace key\n\n'),
          call(
              '// NULL-terminated list of device policy registry key names.\n'),
          call('extern const char* kDevicePolicyKeys[];\n\n'),
          call('''\
// Access to the mutable protobuf function of all supported boolean user
// policies.
'''),
          call('''\
struct BooleanPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::BooleanPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
'''),
          call('extern const BooleanPolicyAccess kBooleanPolicyAccess[];\n\n'),
          call('''\
// Access to the mutable protobuf function of all supported integer user
// policies.
'''),
          call('''\
struct IntegerPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::IntegerPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
'''),
          call('extern const IntegerPolicyAccess kIntegerPolicyAccess[];\n\n'),
          call('''\
// Access to the mutable protobuf function of all supported string user
// policies.
'''),
          call('''\
struct StringPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
'''),
          call('extern const StringPolicyAccess kStringPolicyAccess[];\n\n'),
          call('''\
// Access to the mutable protobuf function of all supported stringlist user
// policies.
'''),
          call('''\
struct StringListPolicyAccess {
  const char* policy_key;
  bool per_profile;
  enterprise_management::StringListPolicyProto*
      (enterprise_management::CloudPolicySettings::*mutable_proto_ptr)();
};
'''),
          call('''\
extern const StringListPolicyAccess kStringListPolicyAccess[];

'''),
          call('''\
}  // namespace policy

#endif  // __BINDINGS_POLICY_CONSTANTS_H_
''')
      ])

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
    with self.subTest():
      mocked_file.assert_called_once_with(output_path, 'w', encoding='utf-8')
      mocked_file().write.assert_has_calls([
          call('''\
#include "bindings/cloud_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

'''),
          call('namespace key {\n\n'),
          call('const char kExampleStringPolicy[] = "ExampleStringPolicy";\n'),
          call('const char kExampleBoolPolicy[] = "ExampleBoolPolicy";\n'),
          call('const char kExampleBoolMergeMetapolicy[] = '
               '"ExampleBoolMergeMetapolicy";\n'),
          call('const char kExampleBoolPrecedenceMetapolicy[] = '
               '"ExampleBoolPrecedenceMetapolicy";\n'),
          call('\n}  // namespace key\n\n'),
          call('const char* kDevicePolicyKeys[] = {\n\n'),
          call('  nullptr};\n\n'),
          call('constexpr BooleanPolicyAccess kBooleanPolicyAccess[] = {\n'),
          call('''\
  {key::kExampleBoolPolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolpolicy},
'''),
          call('''\
  {key::kExampleBoolMergeMetapolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolmergemetapolicy},
'''),
          call('''\
  {key::kExampleBoolPrecedenceMetapolicy,
   false,
   &em::CloudPolicySettings::mutable_exampleboolprecedencemetapolicy},
'''),
          call('  {nullptr, false, nullptr},\n};\n\n'),
          call('constexpr IntegerPolicyAccess kIntegerPolicyAccess[] = {\n'),
          call('  {nullptr, false, nullptr},\n};\n\n'),
          call('constexpr StringPolicyAccess kStringPolicyAccess[] = {\n'),
          call('''\
  {key::kExampleStringPolicy,
   false,
   &em::CloudPolicySettings::mutable_examplestringpolicy},
'''),
          call('  {nullptr, false, nullptr},\n};\n\n'),
          call(
              'constexpr StringListPolicyAccess kStringListPolicyAccess[] = {\n'
          ),
          call('  {nullptr, false, nullptr},\n};\n\n'),
          call('}  // namespace policy\n')
      ])


if __name__ == '__main__':
  unittest.main()

#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''python %prog [options]

Pass at least:
--chrome-version-file <path to src/chrome/VERSION> or --all-chrome-versions
--target-platform <which platform the target code will be generated for and can
  be one of (win, mac, linux, chromeos, fuchsia, ios)>
--policy_templates <path to the policy_templates.json input file>.'''


from __future__ import with_statement
from collections import namedtuple
from collections import OrderedDict
from functools import partial
import json
from optparse import OptionParser
import re
import sys
import textwrap
from xml.sax.saxutils import escape as xml_escape

if sys.version_info.major == 2:
  string_type = basestring
else:
  string_type = str

CHROME_POLICY_KEY = 'SOFTWARE\\\\Policies\\\\Google\\\\Chrome'
CHROMIUM_POLICY_KEY = 'SOFTWARE\\\\Policies\\\\Chromium'


class PolicyDetails:
  """Parses a policy template and caches all its details."""

  # Maps policy types to a tuple with 4 other types:
  # - the equivalent base::Value::Type or 'TYPE_EXTERNAL' if the policy
  #   references external data
  # - the equivalent Protobuf field type
  # - the name of one of the protobufs for shared policy types
  # - the equivalent type in Android's App Restriction Schema
  # TODO(joaodasilva): refactor the 'dict' type into a more generic 'json' type
  # that can also be used to represent lists of other JSON objects.
  TYPE_MAP = {
      'dict': ('Type::DICTIONARY', 'string', 'String', 'string'),
      'external': ('TYPE_EXTERNAL', 'string', 'String', 'invalid'),
      'int': ('Type::INTEGER', 'int64', 'Integer', 'integer'),
      'int-enum': ('Type::INTEGER', 'int64', 'Integer', 'choice'),
      'list': ('Type::LIST', 'StringList', 'StringList', 'string'),
      'main': ('Type::BOOLEAN', 'bool', 'Boolean', 'bool'),
      'string': ('Type::STRING', 'string', 'String', 'string'),
      'string-enum': ('Type::STRING', 'string', 'String', 'choice'),
      'string-enum-list': ('Type::LIST', 'StringList', 'StringList',
                           'multi-select'),
  }

  class EnumItem:

    def __init__(self, item):
      self.caption = PolicyDetails._RemovePlaceholders(item['caption'])
      self.value = item['value']

  def __init__(self, policy, chrome_major_version, target_platform, valid_tags):
    self.id = policy['id']
    self.name = policy['name']
    self.tags = policy.get('tags', None)
    self._CheckTagsValidity(valid_tags)
    features = policy.get('features', {})
    self.can_be_recommended = features.get('can_be_recommended', False)
    self.can_be_mandatory = features.get('can_be_mandatory', True)
    self.is_deprecated = policy.get('deprecated', False)
    self.is_device_only = policy.get('device_only', False)
    self.is_future = policy.get('future', False)
    self.supported_chrome_os_management = policy.get(
        'supported_chrome_os_management', ['active_directory', 'google_cloud'])
    self.schema = policy['schema']
    self.validation_schema = policy.get('validation_schema')
    self.has_enterprise_default = 'default_for_enterprise_users' in policy
    if self.has_enterprise_default:
      self.enterprise_default = policy['default_for_enterprise_users']

    self.platforms = []
    for platform, version_range in [
        p.split(':') for p in policy['supported_on']
    ]:
      if self.is_device_only and platform != 'chrome_os':
        raise RuntimeError(
            'is_device_only is only allowed for Chrome OS: "%s"' % p)
      if platform not in [
          'chrome_frame',
          'chrome_os',
          'android',
          'webview_android',
          'ios',
          'chrome.win',
          'chrome.linux',
          'chrome.mac',
          'chrome.fuchsia',
          'chrome.*',
          'chrome.win7',
      ]:
        raise RuntimeError('Platform "%s" is not supported' % platform)

      split_result = version_range.split('-')
      if len(split_result) != 2:
        raise RuntimeError('supported_on must have exactly one dash: "%s"' % p)
      (version_min, version_max) = split_result
      if version_min == '':
        raise RuntimeError('supported_on must define a start version: "%s"' % p)

      # Skip if filtering by Chromium version and the current Chromium version
      # does not support the policy.
      if chrome_major_version:
        if (int(version_min) > chrome_major_version or
            version_max != '' and int(version_max) < chrome_major_version):
          continue

      if platform.startswith('chrome.'):
        platform_sub = platform[7:]
        if platform_sub == '*':
          self.platforms.extend(['win', 'mac', 'linux', 'fuchsia'])
        elif platform_sub == 'win7':
          self.platforms.append('win')
        else:
          self.platforms.append(platform_sub)
      else:
        self.platforms.append(platform)

    self.platforms.sort()
    self.is_supported = target_platform in self.platforms

    if policy['type'] not in PolicyDetails.TYPE_MAP:
      raise NotImplementedError(
          'Unknown policy type for %s: %s' % (policy['name'], policy['type']))
    self.policy_type, self.protobuf_type, self.policy_protobuf_type, \
        self.restriction_type = PolicyDetails.TYPE_MAP[policy['type']]

    self.desc = '\n'.join(
        map(str.strip,
            PolicyDetails._RemovePlaceholders(policy['desc']).splitlines()))
    self.caption = PolicyDetails._RemovePlaceholders(policy['caption'])
    self.max_size = policy.get('max_size', 0)

    items = policy.get('items')
    if items is None:
      self.items = None
    else:
      self.items = [PolicyDetails.EnumItem(entry) for entry in items]

  PH_PATTERN = re.compile('<ph[^>]*>([^<]*|[^<]*<ex>([^<]*)</ex>[^<]*)</ph>')

  def _CheckTagsValidity(self, valid_tags):
    if self.tags == None:
      raise RuntimeError('Policy ' + self.name + ' has to contain a list of '
                         'tags!\n An empty list is also valid but means '
                         'setting this policy can never harm the user\'s '
                         'privacy or security.\n')
    for tag in self.tags:
      if not tag in valid_tags:
        raise RuntimeError('Invalid Tag:' + tag + '!\n'
                           'Chose a valid tag from \'risk_tag_definitions\' (a '
                           'subproperty of root in policy_templates.json)!')

  # Simplistic grit placeholder stripper.
  @staticmethod
  def _RemovePlaceholders(text):
    result = ''
    pos = 0
    for m in PolicyDetails.PH_PATTERN.finditer(text):
      result += text[pos:m.start(0)]
      result += m.group(2) or m.group(1)
      pos = m.end(0)
    result += text[pos:]
    return result


class PolicyAtomicGroup:
  """Parses a policy atomic group and caches its name and policy names"""

  def __init__(self, policy_group, available_policies,
               policies_already_in_group):
    self.id = policy_group['id']
    self.name = policy_group['name']
    self.policies = policy_group.get('policies', None)
    self._CheckPoliciesValidity(available_policies, policies_already_in_group)

  def _CheckPoliciesValidity(self, available_policies,
                             policies_already_in_group):
    if self.policies == None or len(self.policies) <= 0:
      raise RuntimeError('Atomic policy group ' + self.name +
                         ' has to contain a list of '
                         'policies!\n')
    for policy in self.policies:
      if policy in policies_already_in_group:
        raise RuntimeError('Policy: ' + policy +
                           ' cannot be in more than one atomic group '
                           'in policy_templates.json)!')
      policies_already_in_group.add(policy)
      if not policy in available_policies:
        raise RuntimeError('Invalid policy: ' + policy + ' in atomic group ' +
                           self.name + '.\n')


def ParseVersionFile(version_path):
  chrome_major_version = None
  for line in open(version_path, 'r').readlines():
    key, val = line.rstrip('\r\n').split('=', 1)
    if key == 'MAJOR':
      chrome_major_version = val
      break
  if chrome_major_version is None:
    raise RuntimeError('VERSION file does not contain major version.')
  return int(chrome_major_version)


def main():
  parser = OptionParser(usage=__doc__)
  parser.add_option(
      '--pch',
      '--policy-constants-header',
      dest='header_path',
      help='generate header file of policy constants',
      metavar='FILE')
  parser.add_option(
      '--pcc',
      '--policy-constants-source',
      dest='source_path',
      help='generate source file of policy constants',
      metavar='FILE')
  parser.add_option(
      '--cpp',
      '--cloud-policy-protobuf',
      dest='cloud_policy_proto_path',
      help='generate cloud policy protobuf file',
      metavar='FILE')
  parser.add_option(
      '--cpfrp',
      '--cloud-policy-full-runtime-protobuf',
      dest='cloud_policy_full_runtime_proto_path',
      help='generate cloud policy full runtime protobuf',
      metavar='FILE')
  parser.add_option(
      '--csp',
      '--chrome-settings-protobuf',
      dest='chrome_settings_proto_path',
      help='generate chrome settings protobuf file',
      metavar='FILE')
  parser.add_option(
      '--policy-common-definitions-protobuf',
      dest='policy_common_definitions_proto_path',
      help='policy common definitions protobuf file path',
      metavar='FILE')
  parser.add_option(
      '--policy-common-definitions-full-runtime-protobuf',
      dest='policy_common_definitions_full_runtime_proto_path',
      help='generate policy common definitions full runtime protobuf file',
      metavar='FILE')
  parser.add_option(
      '--csfrp',
      '--chrome-settings-full-runtime-protobuf',
      dest='chrome_settings_full_runtime_proto_path',
      help='generate chrome settings full runtime protobuf',
      metavar='FILE')
  parser.add_option(
      '--ard',
      '--app-restrictions-definition',
      dest='app_restrictions_path',
      help='generate an XML file as specified by '
      'Android\'s App Restriction Schema',
      metavar='FILE')
  parser.add_option(
      '--rth',
      '--risk-tag-header',
      dest='risk_header_path',
      help='generate header file for policy risk tags',
      metavar='FILE')
  parser.add_option(
      '--crospch',
      '--cros-policy-constants-header',
      dest='cros_constants_header_path',
      help='generate header file of policy constants for use in '
      'Chrome OS',
      metavar='FILE')
  parser.add_option(
      '--crospcc',
      '--cros-policy-constants-source',
      dest='cros_constants_source_path',
      help='generate source file of policy constants for use in '
      'Chrome OS',
      metavar='FILE')
  parser.add_option(
      '--chrome-version-file',
      dest='chrome_version_file',
      help='path to src/chrome/VERSION',
      metavar='FILE')
  parser.add_option(
      '--all-chrome-versions',
      action='store_true',
      dest='all_chrome_versions',
      default=False,
      help='do not restrict generated policies by chrome version')
  parser.add_option(
      '--target-platform',
      dest='target_platform',
      help='the platform the generated code should run on - can be one of'
      '(win, mac, linux, chromeos, fuchsia)',
      metavar='PLATFORM')
  parser.add_option(
      '--policy-templates-file',
      dest='policy_templates_file',
      help='path to the policy_templates.json input file',
      metavar='FILE')
  (opts, args) = parser.parse_args()

  has_arg_error = False

  if not opts.target_platform:
    print('Error: Missing --target-platform=<platform>')
    has_arg_error = True

  if not opts.policy_templates_file:
    print('Error: Missing'
          ' --policy-templates-file=<path to policy_templates.json>')
    has_arg_error = True

  if not opts.chrome_version_file and not opts.all_chrome_versions:
    print('Error: Missing'
          ' --chrome-version-file=<path to src/chrome/VERSION>\n'
          ' or --all-chrome-versions')
    has_arg_error = True

  if has_arg_error:
    print('')
    parser.print_help()
    return 2

  version_path = opts.chrome_version_file
  target_platform = opts.target_platform
  template_file_name = opts.policy_templates_file

  # --target-platform accepts "chromeos" as its input because that's what is
  # used within GN. Within policy templates, "chrome_os" is used instead.
  if target_platform == 'chromeos':
    target_platform = 'chrome_os'

  if opts.all_chrome_versions:
    chrome_major_version = None
  else:
    chrome_major_version = ParseVersionFile(version_path)

  template_file_contents = _LoadJSONFile(template_file_name)
  risk_tags = RiskTags(template_file_contents)
  policy_details = [
      PolicyDetails(policy, chrome_major_version, target_platform,
                    risk_tags.GetValidTags())
      for policy in template_file_contents['policy_definitions']
      if policy['type'] != 'group'
  ]
  risk_tags.ComputeMaxTags(policy_details)
  sorted_policy_details = sorted(policy_details, key=lambda policy: policy.name)

  policy_details_set = list(map((lambda x: x.name), policy_details))
  policies_already_in_group = set()
  policy_atomic_groups = [
      PolicyAtomicGroup(group, policy_details_set, policies_already_in_group)
      for group in template_file_contents['policy_atomic_group_definitions']
  ]
  sorted_policy_atomic_groups = sorted(
      policy_atomic_groups, key=lambda group: group.name)


  def GenerateFile(path, writer, sorted=False, xml=False):
    if path:
      with open(path, 'w') as f:
        _OutputGeneratedWarningHeader(f, template_file_name, xml)
        writer(sorted and sorted_policy_details or policy_details,
               sorted and sorted_policy_atomic_groups or policy_atomic_groups,
               target_platform, f, risk_tags)

  if opts.header_path:
    GenerateFile(opts.header_path, _WritePolicyConstantHeader, sorted=True)
  if opts.source_path:
    GenerateFile(opts.source_path, _WritePolicyConstantSource, sorted=True)
  if opts.risk_header_path:
    GenerateFile(opts.risk_header_path, _WritePolicyRiskTagHeader)
  if opts.cloud_policy_proto_path:
    GenerateFile(opts.cloud_policy_proto_path, _WriteCloudPolicyProtobuf)
  if (opts.policy_common_definitions_full_runtime_proto_path and
      opts.policy_common_definitions_proto_path):
    GenerateFile(
        opts.policy_common_definitions_full_runtime_proto_path,
        partial(_WritePolicyCommonDefinitionsFullRuntimeProtobuf,
                opts.policy_common_definitions_proto_path))
  if opts.cloud_policy_full_runtime_proto_path:
    GenerateFile(opts.cloud_policy_full_runtime_proto_path,
                 _WriteCloudPolicyFullRuntimeProtobuf)
  if opts.chrome_settings_proto_path:
    GenerateFile(opts.chrome_settings_proto_path, _WriteChromeSettingsProtobuf)
  if opts.chrome_settings_full_runtime_proto_path:
    GenerateFile(opts.chrome_settings_full_runtime_proto_path,
                 _WriteChromeSettingsFullRuntimeProtobuf)

  if target_platform == 'android' and opts.app_restrictions_path:
    GenerateFile(opts.app_restrictions_path, _WriteAppRestrictions, xml=True)

  # Generated code for Chrome OS (unused in Chromium).
  if opts.cros_constants_header_path:
    GenerateFile(
        opts.cros_constants_header_path,
        _WriteChromeOSPolicyConstantsHeader,
        sorted=True)
  if opts.cros_constants_source_path:
    GenerateFile(
        opts.cros_constants_source_path,
        _WriteChromeOSPolicyConstantsSource,
        sorted=True)

  return 0


#------------------ shared helpers ---------------------------------#


def _OutputGeneratedWarningHeader(f, template_file_path, xml_style):
  left_margin = '//'
  if xml_style:
    left_margin = '    '
    f.write('<?xml version="1.0" encoding="utf-8"?>\n' '<!--\n')
  else:
    f.write('//\n')

  f.write(left_margin + ' DO NOT MODIFY THIS FILE DIRECTLY!\n')
  f.write(left_margin + ' IT IS GENERATED BY generate_policy_source.py\n')
  f.write(left_margin + ' FROM ' + template_file_path + '\n')

  if xml_style:
    f.write('-->\n\n')
  else:
    f.write(left_margin + '\n\n')


COMMENT_WRAPPER = textwrap.TextWrapper()
COMMENT_WRAPPER.width = 80
COMMENT_WRAPPER.initial_indent = '// '
COMMENT_WRAPPER.subsequent_indent = '// '
COMMENT_WRAPPER.replace_whitespace = False


# Writes a comment, each line prefixed by // and wrapped to 80 spaces.
def _OutputComment(f, comment):
  for line in comment.splitlines():
    if len(line) == 0:
      f.write('//')
    else:
      f.write(COMMENT_WRAPPER.fill(line))
    f.write('\n')


def _LoadJSONFile(json_file):
  with open(json_file, 'r') as f:
    text = f.read()
  return eval(text)


#------------------ policy constants header ------------------------#


def _WritePolicyConstantHeader(policies, policy_atomic_groups, target_platform,
                               f, risk_tags):
  f.write('#ifndef CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '#define CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '\n'
          '#include <cstdint>\n'
          '#include <string>\n'
          '\n'
          '#include "base/values.h"\n'
          '#include "components/policy/core/common/policy_details.h"\n'
          '#include "components/policy/core/common/policy_map.h"\n'
          '#include "components/policy/proto/cloud_policy.pb.h"\n'
          '\n'
          'namespace policy {\n'
          '\n'
          'namespace internal {\n'
          'struct SchemaData;\n'
          '}\n\n')

  if target_platform == 'win':
    f.write('// The windows registry path where Chrome policy '
            'configuration resides.\n'
            'extern const wchar_t kRegistryChromePolicyKey[];\n')

  f.write('#if defined (OS_CHROMEOS)\n'
          '// Sets default values for enterprise users.\n'
          'void SetEnterpriseUsersDefaults(PolicyMap* policy_map);\n'
          '#endif\n'
          '\n'
          '// Returns the PolicyDetails for |policy| if |policy| is a known\n'
          '// Chrome policy, otherwise returns NULL.\n'
          'const PolicyDetails* GetChromePolicyDetails('
          'const std::string& policy);\n'
          '\n'
          '// Returns the schema data of the Chrome policy schema.\n'
          'const internal::SchemaData* GetChromeSchemaData();\n'
          '\n')
  f.write('// Key names for the policy settings.\n' 'namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('extern const char k' + policy.name + '[];\n')
  f.write('\n}  // namespace key\n\n')

  f.write('// Group names for the policy settings.\n' 'namespace group {\n\n')
  for group in policy_atomic_groups:
    f.write('extern const char k' + group.name + '[];\n')
  f.write('\n}  // namespace group\n\n')

  f.write('struct AtomicGroup {\n'
          '  const short id;\n'
          '  const char* policy_group;\n'
          '  const char* const* policies;\n'
          '};\n\n')

  f.write('extern const AtomicGroup kPolicyAtomicGroupMappings[];\n\n')
  f.write('extern const size_t kPolicyAtomicGroupMappingsLength;\n\n')

  f.write('enum class StringPolicyType {\n'
          '  STRING,\n'
          '  JSON,\n'
          '  EXTERNAL,\n'
          '};\n\n')

  # User policy proto pointers, one struct for each protobuf type.
  protobuf_types = _GetProtobufTypes(policies)
  for protobuf_type in protobuf_types:
    _WriteChromePolicyAccessHeader(f, protobuf_type)

  f.write('constexpr int64_t kDevicePolicyExternalDataResourceCacheSize = %d;\n'
          % _ComputeTotalDevicePolicyExternalDataMaxSize(policies))

  f.write('\n}  // namespace policy\n\n'
          '#endif  // CHROME_COMMON_POLICY_CONSTANTS_H_\n')


def _WriteChromePolicyAccessHeader(f, protobuf_type):
  f.write('// Read access to the protobufs of all supported %s user policies.\n'
          % protobuf_type.lower())
  f.write('struct %sPolicyAccess {\n' % protobuf_type)
  f.write('  const char* policy_key;\n'
          '  bool (enterprise_management::CloudPolicySettings::'
          '*has_proto)() const;\n'
          '  const enterprise_management::%sPolicyProto&\n'
          '      (enterprise_management::CloudPolicySettings::'
          '*get_proto)() const;\n' % protobuf_type)
  if protobuf_type == 'String':
    f.write('  const StringPolicyType type;\n')
  f.write('};\n')
  f.write('extern const %sPolicyAccess k%sPolicyAccess[];\n\n' %
          (protobuf_type, protobuf_type))


def _ComputeTotalDevicePolicyExternalDataMaxSize(policies):
  total_device_policy_external_data_max_size = 0
  for policy in policies:
    if policy.is_device_only and policy.policy_type == 'TYPE_EXTERNAL':
      total_device_policy_external_data_max_size += policy.max_size
  return total_device_policy_external_data_max_size


#------------------ policy constants source ------------------------#

SchemaNodeKey = namedtuple('SchemaNodeKey',
                           'schema_type extra is_sensitive_value')
SchemaNode = namedtuple(
    'SchemaNode',
    'schema_type extra is_sensitive_value has_sensitive_children comments')
PropertyNode = namedtuple('PropertyNode', 'key schema')
PropertiesNode = namedtuple(
    'PropertiesNode',
    'begin end pattern_end required_begin required_end additional name')
RestrictionNode = namedtuple('RestrictionNode', 'first second')

# A mapping of the simple schema types to base::Value::Types.
SIMPLE_SCHEMA_NAME_MAP = {
    'boolean': 'Type::BOOLEAN',
    'integer': 'Type::INTEGER',
    'null': 'Type::NONE',
    'number': 'Type::DOUBLE',
    'string': 'Type::STRING',
}

INVALID_INDEX = -1
MIN_INDEX = -1
MAX_INDEX = (1 << 15) - 1  # signed short in c++
MIN_POLICY_ID = 0
MAX_POLICY_ID = (1 << 16) - 1  # unsigned short
MIN_EXTERNAL_DATA_SIZE = 0
MAX_EXTERNAL_DATA_SIZE = (1 << 32) - 1  # unsigned int32


class SchemaNodesGenerator:
  """Builds the internal structs to represent a JSON schema."""

  def __init__(self, shared_strings):
    """Creates a new generator.

    |shared_strings| is a map of strings to a C expression that evaluates to
    that string at runtime. This mapping can be used to reuse existing string
    constants."""
    self.shared_strings = shared_strings
    self.key_index_map = {}  # |SchemaNodeKey| -> index in |schema_nodes|
    self.schema_nodes = []  # List of |SchemaNode|s
    self.property_nodes = []  # List of |PropertyNode|s
    self.properties_nodes = []  # List of |PropertiesNode|s
    self.restriction_nodes = []  # List of |RestrictionNode|s
    self.required_properties = []
    self.int_enums = []
    self.string_enums = []
    self.ranges = {}
    self.id_map = {}

  def GetString(self, s):
    if s in self.shared_strings:
      return self.shared_strings[s]
    # Generate JSON escaped string, which is slightly different from desired
    # C/C++ escaped string. Known differences includes unicode escaping format.
    return json.dumps(s)

  def AppendSchema(self, schema_type, extra, is_sensitive_value, comment=''):
    # Find existing schema node with same structure.
    key_node = SchemaNodeKey(schema_type, extra, is_sensitive_value)
    if key_node in self.key_index_map:
      index = self.key_index_map[key_node]
      if comment:
        self.schema_nodes[index].comments.add(comment)
      return index

    # Create new schema node.
    index = len(self.schema_nodes)
    comments = {comment} if comment else set()
    schema_node = SchemaNode(schema_type, extra, is_sensitive_value, False,
                             comments)
    self.schema_nodes.append(schema_node)
    self.key_index_map[key_node] = index
    return index

  def AppendRestriction(self, first, second):
    r = RestrictionNode(str(first), str(second))
    if not r in self.ranges:
      self.ranges[r] = len(self.restriction_nodes)
      self.restriction_nodes.append(r)
    return self.ranges[r]

  def GetSimpleType(self, name, is_sensitive_value):
    return self.AppendSchema(SIMPLE_SCHEMA_NAME_MAP[name], INVALID_INDEX,
                             is_sensitive_value, 'simple type: ' + name)

  def SchemaHaveRestriction(self, schema):
    return any(keyword in schema
               for keyword in ['minimum', 'maximum', 'enum', 'pattern'])

  def IsConsecutiveInterval(self, seq):
    sortedSeq = sorted(seq)
    return all(
        sortedSeq[i] + 1 == sortedSeq[i + 1] for i in range(len(sortedSeq) - 1))

  def GetEnumIntegerType(self, schema, is_sensitive_value, name):
    assert all(type(x) == int for x in schema['enum'])
    possible_values = schema['enum']
    if self.IsConsecutiveInterval(possible_values):
      index = self.AppendRestriction(max(possible_values), min(possible_values))
      return self.AppendSchema(
          'Type::INTEGER', index, is_sensitive_value,
          'integer with enumeration restriction (use range instead): %s' % name)
    offset_begin = len(self.int_enums)
    self.int_enums += possible_values
    offset_end = len(self.int_enums)
    return self.AppendSchema('Type::INTEGER',
                             self.AppendRestriction(offset_begin, offset_end),
                             is_sensitive_value,
                             'integer with enumeration restriction: %s' % name)

  def GetEnumStringType(self, schema, is_sensitive_value, name):
    assert all(type(x) == str for x in schema['enum'])
    offset_begin = len(self.string_enums)
    self.string_enums += schema['enum']
    offset_end = len(self.string_enums)
    return self.AppendSchema('Type::STRING',
                             self.AppendRestriction(offset_begin, offset_end),
                             is_sensitive_value,
                             'string with enumeration restriction: %s' % name)

  def GetEnumType(self, schema, is_sensitive_value, name):
    if len(schema['enum']) == 0:
      raise RuntimeError('Empty enumeration in %s' % name)
    elif schema['type'] == 'integer':
      return self.GetEnumIntegerType(schema, is_sensitive_value, name)
    elif schema['type'] == 'string':
      return self.GetEnumStringType(schema, is_sensitive_value, name)
    else:
      raise RuntimeError('Unknown enumeration type in %s' % name)

  def GetPatternType(self, schema, is_sensitive_value, name):
    if schema['type'] != 'string':
      raise RuntimeError('Unknown pattern type in %s' % name)
    pattern = schema['pattern']
    # Try to compile the pattern to validate it, note that the syntax used
    # here might be slightly different from re2.
    # TODO(binjin): Add a python wrapper of re2 and use it here.
    re.compile(pattern)
    index = len(self.string_enums)
    self.string_enums.append(pattern)
    return self.AppendSchema('Type::STRING', self.AppendRestriction(
        index, index), is_sensitive_value,
                             'string with pattern restriction: %s' % name)

  def GetRangedType(self, schema, is_sensitive_value, name):
    if schema['type'] != 'integer':
      raise RuntimeError('Unknown ranged type in %s' % name)
    min_value_set, max_value_set = False, False
    if 'minimum' in schema:
      min_value = int(schema['minimum'])
      min_value_set = True
    if 'maximum' in schema:
      max_value = int(schema['maximum'])
      max_value_set = True
    if min_value_set and max_value_set and min_value > max_value:
      raise RuntimeError('Invalid ranged type in %s' % name)
    index = self.AppendRestriction(
        str(max_value) if max_value_set else 'INT_MAX',
        str(min_value) if min_value_set else 'INT_MIN')
    return self.AppendSchema('Type::INTEGER', index, is_sensitive_value,
                             'integer with ranged restriction: %s' % name)

  def Generate(self, schema, name):
    """Generates the structs for the given schema.

    |schema|: a valid JSON schema in a dictionary.
    |name|: the name of the current node, for the generated comments."""
    if '$ref' in schema:
      if 'id' in schema:
        raise RuntimeError("Schemas with a $ref can't have an id")
      if not isinstance(schema['$ref'], string_type):
        raise RuntimeError("$ref attribute must be a string")
      return schema['$ref']

    is_sensitive_value = schema.get('sensitiveValue', False)
    assert type(is_sensitive_value) is bool

    if schema['type'] in SIMPLE_SCHEMA_NAME_MAP:
      if not self.SchemaHaveRestriction(schema):
        # Simple types use shared nodes.
        return self.GetSimpleType(schema['type'], is_sensitive_value)
      elif 'enum' in schema:
        return self.GetEnumType(schema, is_sensitive_value, name)
      elif 'pattern' in schema:
        return self.GetPatternType(schema, is_sensitive_value, name)
      else:
        return self.GetRangedType(schema, is_sensitive_value, name)

    if schema['type'] == 'array':
      return self.AppendSchema(
          'Type::LIST',
          self.GenerateAndCollectID(schema['items'], 'items of ' + name),
          is_sensitive_value)
    elif schema['type'] == 'object':
      # Reserve an index first, so that dictionaries come before their
      # properties. This makes sure that the root node is the first in the
      # SchemaNodes array.
      # This however, prevents de-duplication for object schemas since we could
      # only determine duplicates after all child schema nodes are generated as
      # well and then we couldn't remove the newly created schema node without
      # invalidating all child schema indices.
      index = len(self.schema_nodes)
      self.schema_nodes.append(
          SchemaNode('Type::DICTIONARY', INVALID_INDEX, is_sensitive_value,
                     False, {name}))

      if 'additionalProperties' in schema:
        additionalProperties = self.GenerateAndCollectID(
            schema['additionalProperties'], 'additionalProperties of ' + name)
      else:
        additionalProperties = INVALID_INDEX

      # Properties must be sorted by name, for the binary search lookup.
      # Note that |properties| must be evaluated immediately, so that all the
      # recursive calls to Generate() append the necessary child nodes; if
      # |properties| were a generator then this wouldn't work.
      sorted_properties = sorted(schema.get('properties', {}).items())
      properties = [
          PropertyNode(
              self.GetString(key), self.GenerateAndCollectID(subschema, key))
          for key, subschema in sorted_properties
      ]

      pattern_properties = []
      for pattern, subschema in schema.get('patternProperties', {}).items():
        pattern_properties.append(
            PropertyNode(
                self.GetString(pattern),
                self.GenerateAndCollectID(subschema, pattern)))

      begin = len(self.property_nodes)
      self.property_nodes += properties
      end = len(self.property_nodes)
      self.property_nodes += pattern_properties
      pattern_end = len(self.property_nodes)

      if index == 0:
        self.root_properties_begin = begin
        self.root_properties_end = end

      required_begin = len(self.required_properties)
      required_properties = schema.get('required', [])
      assert type(required_properties) is list
      assert all(type(x) == str for x in required_properties)
      self.required_properties += required_properties
      required_end = len(self.required_properties)

      # Check that each string in |required_properties| is in |properties|.
      properties = schema.get('properties', {})
      for name in required_properties:
        assert name in properties

      extra = len(self.properties_nodes)
      self.properties_nodes.append(
          PropertiesNode(begin, end, pattern_end, required_begin, required_end,
                         additionalProperties, name))

      # Update index at |extra| now, since that was filled with a dummy value
      # when the schema node was created.
      self.schema_nodes[index] = self.schema_nodes[index]._replace(extra=extra)
      return index
    else:
      assert False

  def GenerateAndCollectID(self, schema, name):
    """A wrapper of Generate(), will take the return value, check and add 'id'
    attribute to self.id_map. The wrapper needs to be used for every call to
    Generate().
    """
    index = self.Generate(schema, name)
    if 'id' not in schema:
      return index
    id_str = schema['id']
    if id_str in self.id_map:
      raise RuntimeError('Duplicated id: ' + id_str)
    self.id_map[id_str] = index
    return index

  def Write(self, f):
    """Writes the generated structs to the given file.

    |f| an open file to write to."""
    f.write('const internal::SchemaNode kSchemas[] = {\n'
            '//  Type' + ' ' * 27 +
            'Extra  IsSensitiveValue HasSensitiveChildren\n')
    for schema_node in self.schema_nodes:
      assert schema_node.extra >= MIN_INDEX and schema_node.extra <= MAX_INDEX
      comment = ('\n' + ' ' * 69 + '// ').join(schema_node.comments)
      f.write('  { base::Value::%-19s %4s %-16s %-5s },  // %s\n' %
              (schema_node.schema_type + ',', str(schema_node.extra) + ',',
               str(schema_node.is_sensitive_value).lower() + ',',
               str(schema_node.has_sensitive_children).lower(), comment))
    f.write('};\n\n')

    if self.property_nodes:
      f.write('const internal::PropertyNode kPropertyNodes[] = {\n'
              '//  Property' + ' ' * 61 + 'Schema\n')
      for property_node in self.property_nodes:
        f.write('  { %-64s %6d },\n' % (property_node.key + ',',
                                        property_node.schema))
      f.write('};\n\n')

    if self.properties_nodes:
      f.write('const internal::PropertiesNode kProperties[] = {\n'
              '//  Begin    End  PatternEnd  RequiredBegin  RequiredEnd'
              '  Additional Properties\n')
      for properties_node in self.properties_nodes:
        for i in range(0, len(properties_node) - 1):
          assert (properties_node[i] >= MIN_INDEX and
                  properties_node[i] <= MAX_INDEX)
        f.write(
            '  { %5d, %5d, %5d, %5d, %10d, %5d },  // %s\n' % properties_node)
      f.write('};\n\n')

    if self.restriction_nodes:
      f.write('const internal::RestrictionNode kRestrictionNodes[] = {\n')
      f.write('//   FIRST, SECOND\n')
      for restriction_node in self.restriction_nodes:
        f.write('  {{ %-8s %4s}},\n' % (restriction_node.first + ',',
                                        restriction_node.second))
      f.write('};\n\n')

    if self.required_properties:
      f.write('const char* const kRequiredProperties[] = {\n')
      for required_property in self.required_properties:
        f.write('  %s,\n' % self.GetString(required_property))
      f.write('};\n\n')

    if self.int_enums:
      f.write('const int kIntegerEnumerations[] = {\n')
      for possible_values in self.int_enums:
        f.write('  %d,\n' % possible_values)
      f.write('};\n\n')

    if self.string_enums:
      f.write('const char* const kStringEnumerations[] = {\n')
      for possible_values in self.string_enums:
        f.write('  %s,\n' % self.GetString(possible_values))
      f.write('};\n\n')

    f.write('const internal::SchemaData kChromeSchemaData = {\n'
            '  kSchemas,\n')
    f.write('  kPropertyNodes,\n' if self.property_nodes else '  NULL,\n')
    f.write('  kProperties,\n' if self.properties_nodes else '  NULL,\n')
    f.write('  kRestrictionNodes,\n' if self.restriction_nodes else '  NULL,\n')
    f.write('  kRequiredProperties,\n' if self
            .required_properties else '  NULL,\n')
    f.write('  kIntegerEnumerations,\n' if self.int_enums else '  NULL,\n')
    f.write('  kStringEnumerations,\n' if self.string_enums else '  NULL,\n')
    f.write('  %d,  // validation_schema root index\n' %
            self.validation_schema_root_index)
    f.write('};\n\n')

  def GetByID(self, id_str):
    if not isinstance(id_str, string_type):
      return id_str
    if id_str not in self.id_map:
      raise RuntimeError('Invalid $ref: ' + id_str)
    return self.id_map[id_str]

  def ResolveID(self, index, tuple_type, params):
    simple_tuple = params[:index] + (self.GetByID(
        params[index]),) + params[index + 1:]
    return tuple_type(*simple_tuple)

  def ResolveReferences(self):
    """Resolve reference mapping, required to be called after Generate()

    After calling Generate(), the type of indices used in schema structures
    might be either int or string. An int type suggests that it's a resolved
    index, but for string type it's unresolved. Resolving a reference is as
    simple as looking up for corresponding ID in self.id_map, and replace the
    old index with the mapped index.
    """
    self.schema_nodes = list(
        map(partial(self.ResolveID, 1, SchemaNode), self.schema_nodes))
    self.property_nodes = list(
        map(partial(self.ResolveID, 1, PropertyNode), self.property_nodes))
    self.properties_nodes = list(
        map(partial(self.ResolveID, 3, PropertiesNode), self.properties_nodes))

  def FindSensitiveChildren(self):
    """Wrapper function, which calls FindSensitiveChildrenRecursive().
    """
    if self.schema_nodes:
      self.FindSensitiveChildrenRecursive(0, set())

  def FindSensitiveChildrenRecursive(self, index, handled_schema_nodes):
    """Recursively compute |has_sensitive_children| for the schema node at
    |index| and all its child elements. A schema has sensitive children if any
    of its children has |is_sensitive_value|==True or has sensitive children
    itself.
    """
    node = self.schema_nodes[index]
    if index in handled_schema_nodes:
      return node.has_sensitive_children or node.is_sensitive_value

    handled_schema_nodes.add(index)
    has_sensitive_children = False
    if node.schema_type == 'Type::DICTIONARY':
      properties_node = self.properties_nodes[node.extra]
      # Iterate through properties and patternProperties.
      for property_index in range(properties_node.begin,
                                  properties_node.pattern_end - 1):
        sub_index = self.property_nodes[property_index].schema
        has_sensitive_children |= self.FindSensitiveChildrenRecursive(
            sub_index, handled_schema_nodes)
      # AdditionalProperties
      if properties_node.additional != INVALID_INDEX:
        sub_index = properties_node.additional
        has_sensitive_children |= self.FindSensitiveChildrenRecursive(
            sub_index, handled_schema_nodes)
    elif node.schema_type == 'Type::LIST':
      sub_index = node.extra
      has_sensitive_children |= self.FindSensitiveChildrenRecursive(
          sub_index, handled_schema_nodes)

    if has_sensitive_children:
      self.schema_nodes[index] = self.schema_nodes[index]._replace(
          has_sensitive_children=True)

    return has_sensitive_children or node.is_sensitive_value


def _GenerateDefaultValue(value):
  """Converts a JSON object into a base::Value entry. Returns a tuple, the first
  entry being a list of declaration statements to define the variable, the
  second entry being a way to access the variable.

  If no definition is needed, the first return value will be an empty list. If
  any error occurs, the second return value will be None (ie, no way to fetch
  the value).

  |value|: The deserialized value to convert to base::Value."""
  if type(value) == bool or type(value) == int:
    return [], 'std::make_unique<base::Value>(%s)' % json.dumps(value)
  elif type(value) == str:
    return [], 'std::make_unique<base::Value>("%s")' % value
  elif type(value) == list:
    setup = ['auto default_value = std::make_unique<base::ListValue>();']
    for entry in value:
      decl, fetch = _GenerateDefaultValue(entry)
      # Nested lists are not supported.
      if decl:
        return [], None
      setup.append('default_value->Append(%s);' % fetch)
    return setup, 'std::move(default_value)'
  return [], None


def _WritePolicyConstantSource(policies, policy_atomic_groups, target_platform,
                               f, risk_tags):
  f.write('#include "components/policy/policy_constants.h"\n'
          '\n'
          '#include <algorithm>\n'
          '#include <climits>\n'
          '#include <memory>\n'
          '\n'
          '#include "base/logging.h"\n'
          '#include "base/stl_util.h"  // base::size()\n'
          '#include "build/branding_buildflags.h"\n'
          '#include "components/policy/core/common/policy_types.h"\n'
          '#include "components/policy/core/common/schema_internal.h"\n'
          '#include "components/policy/proto/cloud_policy.pb.h"\n'
          '#include "components/policy/risk_tag.h"\n'
          '\n'
          'namespace em = enterprise_management;\n\n'
          '\n'
          'namespace policy {\n'
          '\n')

  # Generate the Chrome schema.
  chrome_schema = {
      'type': 'object',
      'properties': {},
  }
  chrome_validation_schema = {
      'type': 'object',
      'properties': {},
  }
  shared_strings = {}
  for policy in policies:
    shared_strings[policy.name] = "key::k%s" % policy.name
    if policy.is_supported:
      chrome_schema['properties'][policy.name] = policy.schema
      if policy.validation_schema is not None:
        (chrome_validation_schema['properties'][policy.name]
        ) = policy.validation_schema

  # Note: this list must be kept in sync with the known property list of the
  # Chrome schema, so that binary searching in the PropertyNode array gets the
  # right index on this array as well. See the implementation of
  # GetChromePolicyDetails() below.
  f.write('const PolicyDetails kChromePolicyDetails[] = {\n'
          '//  is_deprecated  is_device_policy  id    max_external_data_size\n')
  for policy in policies:
    if policy.is_supported:
      assert policy.id >= MIN_POLICY_ID and policy.id <= MAX_POLICY_ID
      assert (policy.max_size >= MIN_EXTERNAL_DATA_SIZE and
              policy.max_size <= MAX_EXTERNAL_DATA_SIZE)
      f.write('  // %s\n' % policy.name)
      f.write('  { %-14s %-16s %3s, %24s,\n'
              '    %s },\n' % ('true,' if policy.is_deprecated else 'false,',
                               'true,' if policy.is_device_only else 'false,',
                               policy.id, policy.max_size,
                               risk_tags.ToInitString(policy.tags)))
  f.write('};\n\n')

  schema_generator = SchemaNodesGenerator(shared_strings)
  schema_generator.GenerateAndCollectID(chrome_schema, 'root node')

  if chrome_validation_schema['properties']:
    schema_generator.validation_schema_root_index = \
        schema_generator.GenerateAndCollectID(chrome_validation_schema,
                                              'validation_schema root node')
  else:
    schema_generator.validation_schema_root_index = INVALID_INDEX

  schema_generator.ResolveReferences()
  schema_generator.FindSensitiveChildren()
  schema_generator.Write(f)

  f.write('\n' 'namespace {\n')

  f.write('bool CompareKeys(const internal::PropertyNode& node,\n'
          '                 const std::string& key) {\n'
          '  return node.key < key;\n'
          '}\n\n')

  f.write('}  // namespace\n\n')

  if target_platform == 'win':
    f.write('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)\n'
            'const wchar_t kRegistryChromePolicyKey[] = '
            'L"' + CHROME_POLICY_KEY + '";\n'
            '#else\n'
            'const wchar_t kRegistryChromePolicyKey[] = '
            'L"' + CHROMIUM_POLICY_KEY + '";\n'
            '#endif\n\n')

  f.write('const internal::SchemaData* GetChromeSchemaData() {\n'
          '  return &kChromeSchemaData;\n'
          '}\n\n')

  f.write('#if defined (OS_CHROMEOS)\n'
          'void SetEnterpriseUsersDefaults(PolicyMap* policy_map) {\n')

  for policy in policies:
    if policy.has_enterprise_default and policy.is_supported:
      declare_default_stmts, fetch_default = _GenerateDefaultValue(
          policy.enterprise_default)
      if not fetch_default:
        raise RuntimeError(
            'Type %s of policy %s is not supported at '
            'enterprise defaults' % (policy.policy_type, policy.name))

      # Convert declare_default_stmts to a string with the correct identation.
      if declare_default_stmts:
        declare_default = '    %s\n' % '\n    '.join(declare_default_stmts)
      else:
        declare_default = ''

      f.write(
          '  if (!policy_map->Get(key::k%s)) {\n'
          '%s'
          '    policy_map->Set(key::k%s,\n'
          '                    POLICY_LEVEL_MANDATORY,\n'
          '                    POLICY_SCOPE_USER,\n'
          '                    POLICY_SOURCE_ENTERPRISE_DEFAULT,\n'
          '                    %s,\n'
          '                    nullptr);\n'
          '  }\n' % (policy.name, declare_default, policy.name, fetch_default))

  f.write('}\n' '#endif\n\n')

  f.write('const PolicyDetails* GetChromePolicyDetails('
          'const std::string& policy) {\n'
          '  // First index in kPropertyNodes of the Chrome policies.\n'
          '  static const int begin_index = %s;\n'
          '  // One-past-the-end of the Chrome policies in kPropertyNodes.\n'
          '  static const int end_index = %s;\n' %
          (schema_generator.root_properties_begin,
           schema_generator.root_properties_end))
  f.write('  const internal::PropertyNode* begin =\n'
          '      kPropertyNodes + begin_index;\n'
          '  const internal::PropertyNode* end = kPropertyNodes + end_index;\n'
          '  const internal::PropertyNode* it =\n'
          '      std::lower_bound(begin, end, policy, CompareKeys);\n'
          '  if (it == end || it->key != policy)\n'
          '    return NULL;\n'
          '  // This relies on kPropertyNodes from begin_index to end_index\n'
          '  // having exactly the same policies (and in the same order) as\n'
          '  // kChromePolicyDetails, so that binary searching on the first\n'
          '  // gets the same results as a binary search on the second would.\n'
          '  // However, kPropertyNodes has the policy names and\n'
          '  // kChromePolicyDetails doesn\'t, so we obtain the index into\n'
          '  // the second array by searching the first to avoid duplicating\n'
          '  // the policy name pointers.\n'
          '  // Offsetting |it| from |begin| here obtains the index we\'re\n'
          '  // looking for.\n'
          '  size_t index = it - begin;\n'
          '  CHECK_LT(index, base::size(kChromePolicyDetails));\n'
          '  return kChromePolicyDetails + index;\n'
          '}\n\n')

  f.write('namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('const char k{name}[] = "{name}";\n'.format(name=policy.name))
  f.write('\n}  // namespace key\n\n')

  f.write('namespace group {\n\n')
  for group in policy_atomic_groups:
    f.write('const char k{name}[] = "{name}";\n'.format(name=group.name))
  f.write('\n')
  f.write('namespace {\n\n')
  for group in policy_atomic_groups:
    f.write('const char* const %s[] = {' % (group.name))
    for policy in group.policies:
      f.write('key::k%s, ' % (policy))
    f.write('nullptr};\n')
  f.write('\n}  // namespace\n')
  f.write('\n}  // namespace group\n\n')

  atomic_groups_length = 0
  f.write('const AtomicGroup kPolicyAtomicGroupMappings[] = {\n')
  for group in policy_atomic_groups:
    atomic_groups_length += 1
    f.write('  {')
    f.write('  {id}, group::k{name}, group::{name}'.format(
        id=group.id, name=group.name))
    f.write('  },\n')
  f.write('};\n\n')
  f.write('const size_t kPolicyAtomicGroupMappingsLength = %s;\n\n' %
          (atomic_groups_length))

  supported_user_policies = [
      p for p in policies if p.is_supported and not p.is_device_only
  ]
  protobuf_types = _GetProtobufTypes(supported_user_policies)
  for protobuf_type in protobuf_types:
    _WriteChromePolicyAccessSource(supported_user_policies, f, protobuf_type)

  f.write('\n}  // namespace policy\n')


# Return the StringPolicyType enum value for a particular policy type.
def _GetStringPolicyType(policy_type):
  if policy_type == 'Type::STRING':
    return 'StringPolicyType::STRING'
  elif policy_type == 'Type::DICTIONARY':
    return 'StringPolicyType::JSON'
  elif policy_type == 'TYPE_EXTERNAL':
    return 'StringPolicyType::EXTERNAL'
  raise RuntimeError('Invalid string type: ' + policy_type + '!\n')


# Writes an array that contains the pointers to the proto field for each policy
# in |policies| of the given |protobuf_type|.
def _WriteChromePolicyAccessSource(policies, f, protobuf_type):
  f.write('const %sPolicyAccess k%sPolicyAccess[] = {\n' % (protobuf_type,
                                                            protobuf_type))
  extra_args = ''
  for policy in policies:
    if policy.policy_protobuf_type == protobuf_type:
      name = policy.name
      if protobuf_type == 'String':
        extra_args = ',\n   ' + _GetStringPolicyType(policy.policy_type)
      f.write('  {key::k%s,\n'
              '   &em::CloudPolicySettings::has_%s,\n'
              '   &em::CloudPolicySettings::%s%s},\n' %
              (name, name.lower(), name.lower(), extra_args))
  # The list is nullptr-terminated.
  f.write('  {nullptr, nullptr, nullptr},\n' '};\n\n')


#------------------ policy risk tag header -------------------------#


class RiskTags(object):
  '''Generates files and strings to translate the parsed risk tags.'''

  # TODO(fhorschig|tnagel): Add, Check & Generate translation descriptions.

  def __init__(self, template_file_contents):
    self.max_tags = None
    self.enum_for_tag = OrderedDict()  # Ordered by severity as stated in JSON.
    self._ReadRiskTagMetaData(template_file_contents)

  def GenerateEnum(self):
    values = ['  ' + self.enum_for_tag[tag] for tag in self.enum_for_tag]
    values.append('  RISK_TAG_COUNT')
    values.append('  RISK_TAG_NONE')
    enum_text = 'enum RiskTag {\n'
    enum_text += ',\n'.join(values) + '\n};\n'
    return enum_text

  def GetMaxTags(self):
    return str(self.max_tags)

  def GetValidTags(self):
    return [tag for tag in self.enum_for_tag]

  def ToInitString(self, tags):
    all_tags = [self._ToEnum(tag) for tag in tags]
    all_tags += ["RISK_TAG_NONE" for missing in range(len(tags), self.max_tags)]
    str_tags = "{ " + ", ".join(all_tags) + " }"
    return "\n    ".join(textwrap.wrap(str_tags, 69))

  def ComputeMaxTags(self, policies):
    self.max_tags = 0
    for policy in policies:
      if not policy.is_supported or policy.tags == None:
        continue
      self.max_tags = max(len(policy.tags), self.max_tags)

  def _ToEnum(self, tag):
    if tag in self.enum_for_tag:
      return self.enum_for_tag[tag]
    raise RuntimeError('Invalid Tag:' + tag + '!\n'
                       'Chose a valid tag from \'risk_tag_definitions\' (a '
                       'subproperty of root in policy_templates.json)!')

  def _ReadRiskTagMetaData(self, template_file_contents):
    for tag in template_file_contents['risk_tag_definitions']:
      if tag.get('name', None) == None:
        raise RuntimeError('Tag in \'risk_tag_definitions\' without '
                           'description found!')
      if tag.get('description', None) == None:
        raise RuntimeError('Tag ' + tag['name'] + ' has no description!')
      if tag.get('user-description', None) == None:
        raise RuntimeError('Tag ' + tag['name'] + ' has no user-description!')
      self.enum_for_tag[tag['name']] = "RISK_TAG_" + tag['name'].replace(
          "-", "_").upper()


def _WritePolicyRiskTagHeader(policies, policy_atomic_groups, target_platform,
                              f, risk_tags):
  f.write('#ifndef CHROME_COMMON_POLICY_RISK_TAG_H_\n'
          '#define CHROME_COMMON_POLICY_RISK_TAG_H_\n'
          '\n'
          '#include <stddef.h>\n'
          '\n'
          'namespace policy {\n'
          '\n' +
          '// The tag of a policy indicates which impact a policy can have on\n'
          '// a user\'s privacy and/or security. Ordered descending by \n'
          '// impact.\n'
          '// The explanation of the single tags is stated in\n'
          '// policy_templates.json within the \'risk_tag_definitions\' tag.'
          '\n' + risk_tags.GenerateEnum() + '\n'
          '// This constant describes how many risk tags were used by the\n'
          '// policy which uses the most risk tags. \n'
          'const size_t kMaxRiskTagCount = ' + risk_tags.GetMaxTags() + ';\n'
          '\n'
          '}  // namespace policy\n'
          '\n'
          '#endif  // CHROME_COMMON_POLICY_RISK_TAG_H_'
          '\n')


#------------------ policy protobufs -------------------------------#

# This code applies to both Active Directory and Google cloud management.

CHROME_SETTINGS_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

// For StringList and PolicyOptions.
import "policy_common_definitions.proto";

'''

CLOUD_POLICY_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

import "policy_common_definitions.proto";
'''

# Field IDs [1..RESERVED_IDS] will not be used in the wrapping protobuf.
RESERVED_IDS = 2


def _WritePolicyProto(f, policy, fields):
  _OutputComment(f, policy.caption + '\n\n' + policy.desc)
  if policy.items is not None:
    _OutputComment(f, '\nValid values:')
    for item in policy.items:
      _OutputComment(f, '  %s: %s' % (str(item.value), item.caption))
  if policy.policy_type == 'Type::DICTIONARY':
    _OutputComment(
        f, '\nValue schema:\n%s' % json.dumps(
            policy.schema, sort_keys=True, indent=4, separators=(',', ': ')))
  _OutputComment(f, '\nSupported on: %s' % ', '.join(policy.platforms))
  if policy.can_be_recommended and not policy.can_be_mandatory:
    _OutputComment(
        f, '\nNote: this policy must have a RECOMMENDED ' +
        'PolicyMode set in PolicyOptions.')
  f.write('message %sProto {\n' % policy.name)
  f.write('  optional PolicyOptions policy_options = 1;\n')
  f.write('  optional %s %s = 2;\n' % (policy.protobuf_type, policy.name))
  f.write('}\n\n')
  fields += [
      '  optional %sProto %s = %s;\n' % (policy.name, policy.name,
                                         policy.id + RESERVED_IDS)
  ]


def _WriteChromeSettingsProtobuf(policies, policy_atomic_groups,
                                 target_platform, f, risk_tags):
  f.write(CHROME_SETTINGS_PROTO_HEAD)
  fields = []
  f.write('// PBs for individual settings.\n\n')
  for policy in policies:
    # Note: This protobuf also gets the unsupported policies, since it's an
    # exhaustive list of all the supported user policies on any platform.
    if not policy.is_device_only:
      _WritePolicyProto(f, policy, fields)

  f.write('// --------------------------------------------------\n'
          '// Big wrapper PB containing the above groups.\n\n'
          'message ChromeSettingsProto {\n')
  f.write(''.join(fields))
  f.write('}\n\n')


def _WriteChromeSettingsFullRuntimeProtobuf(policies, policy_atomic_groups,
                                            target_platform, f, risk_tags):
  # For full runtime, disable LITE_RUNTIME switch and import full runtime
  # version of cloud_policy.proto.
  f.write(
      CHROME_SETTINGS_PROTO_HEAD.replace(
          "option optimize_for = LITE_RUNTIME;",
          "//option optimize_for = LITE_RUNTIME;").replace(
              "import \"cloud_policy.proto\";",
              "import \"cloud_policy_full_runtime.proto\";").replace(
                  "import \"policy_common_definitions.proto\";",
                  "import \"policy_common_definitions_full_runtime.proto\";"))
  fields = []
  f.write('// PBs for individual settings.\n\n')
  for policy in policies:
    # Note: This protobuf also gets the unsupported policies, since it's an
    # exhaustive list of all the supported user policies on any platform.
    if not policy.is_device_only:
      _WritePolicyProto(f, policy, fields)

  f.write('// --------------------------------------------------\n'
          '// Big wrapper PB containing the above groups.\n\n'
          'message ChromeSettingsProto {\n')
  f.write(''.join(fields))
  f.write('}\n\n')


def _WriteCloudPolicyProtobuf(policies, policy_atomic_groups, target_platform,
                              f, risk_tags):
  f.write(CLOUD_POLICY_PROTO_HEAD)
  f.write('message CloudPolicySettings {\n')
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      f.write(
          '  optional %sPolicyProto %s = %s;\n' %
          (policy.policy_protobuf_type, policy.name, policy.id + RESERVED_IDS))
  f.write('}\n\n')


def _WriteCloudPolicyFullRuntimeProtobuf(policies, policy_atomic_groups,
                                         target_platform, f, risk_tags):
  # For full runtime, disable LITE_RUNTIME switch
  f.write(
      CLOUD_POLICY_PROTO_HEAD.replace(
          "option optimize_for = LITE_RUNTIME;",
          "//option optimize_for = LITE_RUNTIME;").replace(
              "import \"policy_common_definitions.proto\";",
              "import \"policy_common_definitions_full_runtime.proto\";"))
  f.write('message CloudPolicySettings {\n')
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      f.write(
          '  optional %sPolicyProto %s = %s;\n' %
          (policy.policy_protobuf_type, policy.name, policy.id + RESERVED_IDS))
  f.write('}\n\n')


def _WritePolicyCommonDefinitionsFullRuntimeProtobuf(
    policy_common_definitions_proto_path, policies, policy_atomic_groups,
    target_platform, f, risk_tags):
  # For full runtime, disable LITE_RUNTIME switch
  with open(policy_common_definitions_proto_path, 'r') as proto_file:
    policy_common_definitions_proto_code = proto_file.read()
  f.write(
      policy_common_definitions_proto_code.replace(
          "option optimize_for = LITE_RUNTIME;",
          "//option optimize_for = LITE_RUNTIME;"))


#------------------ Chrome OS policy constants header --------------#

# This code applies to Active Directory management only.


# Filter for _GetSupportedChromeOSPolicies().
def _IsSupportedChromeOSPolicy(type, policy):
  # Filter out unsupported policies.
  if not policy.is_supported:
    return False
  # Filter out device policies if user policies are requested.
  if type == 'user' and policy.is_device_only:
    return False
  # Filter out user policies if device policies are requested.
  if type == 'device' and not policy.is_device_only:
    return False
  # Filter out non-Active-Directory policies.
  if 'active_directory' not in policy.supported_chrome_os_management:
    return False
  return True


# Returns a list of supported user and/or device policies `by filtering
# |policies|. |type| may be 'user', 'device' or 'both'.
def _GetSupportedChromeOSPolicies(policies, type):
  if (type not in ['user', 'device', 'both']):
    raise RuntimeError('Unsupported type "%s"' % type)

  return filter(partial(_IsSupportedChromeOSPolicy, type), policies)


# Returns the set of all policy.policy_protobuf_type strings from |policies|.
def _GetProtobufTypes(policies):
  return set(policy.policy_protobuf_type for policy in policies)


# Writes the definition of an array that contains the pointers to the mutable
# proto field for each policy in |policies| of the given |protobuf_type|.
def _WriteChromeOSPolicyAccessHeader(f, protobuf_type):
  f.write('// Access to the mutable protobuf function of all supported '
          '%s user\n// policies.\n' % protobuf_type.lower())
  f.write('struct %sPolicyAccess {\n'
          '  const char* policy_key;\n'
          '  enterprise_management::%sPolicyProto*\n'
          '      (enterprise_management::CloudPolicySettings::'
          '*mutable_proto_ptr)();\n'
          '};\n' % (protobuf_type, protobuf_type))
  f.write('extern const %sPolicyAccess k%sPolicyAccess[];\n\n' %
          (protobuf_type, protobuf_type))


# Writes policy_constants.h for use in Chrome OS.
def _WriteChromeOSPolicyConstantsHeader(policies, policy_atomic_groups,
                                        target_platform, f, risk_tags):
  f.write('#ifndef __BINDINGS_POLICY_CONSTANTS_H_\n'
          '#define __BINDINGS_POLICY_CONSTANTS_H_\n\n')

  # Forward declarations.
  supported_user_policies = _GetSupportedChromeOSPolicies(policies, 'user')
  protobuf_types = _GetProtobufTypes(supported_user_policies)
  f.write('namespace enterprise_management {\n' 'class CloudPolicySettings;\n')
  for protobuf_type in protobuf_types:
    f.write('class %sPolicyProto;\n' % protobuf_type)
  f.write('}  // namespace enterprise_management\n\n')

  f.write('namespace policy {\n\n')

  # Policy keys.
  all_supported_policies = _GetSupportedChromeOSPolicies(policies, 'both')
  f.write('// Registry key names for user and device policies.\n'
          'namespace key {\n\n')
  for policy in all_supported_policies:
    f.write('extern const char k' + policy.name + '[];\n')
  f.write('\n}  // namespace key\n\n')

  # Device policy keys.
  f.write('// NULL-terminated list of device policy registry key names.\n')
  f.write('extern const char* kDevicePolicyKeys[];\n\n')

  # User policy proto pointers, one struct for each protobuf type.
  for protobuf_type in protobuf_types:
    _WriteChromeOSPolicyAccessHeader(f, protobuf_type)

  f.write('}  // namespace policy\n\n'
          '#endif  // __BINDINGS_POLICY_CONSTANTS_H_\n')


#------------------ Chrome OS policy constants source --------------#


# Writes an array that contains the pointers to the mutable proto field for each
# policy in |policies| of the given |protobuf_type|.
def _WriteChromeOSPolicyAccessSource(policies, f, protobuf_type):
  f.write('constexpr %sPolicyAccess k%sPolicyAccess[] = {\n' % (protobuf_type,
                                                                protobuf_type))
  for policy in policies:
    if policy.policy_protobuf_type == protobuf_type:
      f.write('  {key::k%s,\n'
              '   &em::CloudPolicySettings::mutable_%s},\n' %
              (policy.name, policy.name.lower()))
  # The list is nullptr-terminated.
  f.write('  {nullptr, nullptr},\n' '};\n\n')


# Writes policy_constants.cc for use in Chrome OS.
def _WriteChromeOSPolicyConstantsSource(policies, policy_atomic_groups,
                                        target_platform, f, risk_tags):
  f.write('#include "bindings/cloud_policy.pb.h"\n'
          '#include "bindings/policy_constants.h"\n\n'
          'namespace em = enterprise_management;\n\n'
          'namespace policy {\n\n')

  # Policy keys.
  all_supported_policies = _GetSupportedChromeOSPolicies(policies, 'both')
  f.write('namespace key {\n\n')
  for policy in all_supported_policies:
    f.write('const char k{name}[] = "{name}";\n'.format(name=policy.name))
  f.write('\n}  // namespace key\n\n')

  # Device policy keys.
  supported_device_policies = _GetSupportedChromeOSPolicies(policies, 'device')
  f.write('const char* kDevicePolicyKeys[] = {\n\n')
  for policy in supported_device_policies:
    f.write('  key::k%s,\n' % policy.name)
  f.write('  nullptr};\n\n')

  # User policy proto pointers, one struct for each protobuf type.
  supported_user_policies = _GetSupportedChromeOSPolicies(policies, 'user')
  protobuf_types = _GetProtobufTypes(supported_user_policies)
  for protobuf_type in protobuf_types:
    _WriteChromeOSPolicyAccessSource(supported_user_policies, f, protobuf_type)

  f.write('}  // namespace policy\n')


#------------------ app restrictions -------------------------------#


def _WriteAppRestrictions(policies, policy_atomic_groups, target_platform, f,
                          risk_tags):

  def WriteRestrictionCommon(key):
    f.write('    <restriction\n' '        android:key="%s"\n' % key)
    f.write('        android:title="@string/%sTitle"\n' % key)
    f.write('        android:description="@string/%sDesc"\n' % key)

  def WriteItemsDefinition(key):
    f.write('        android:entries="@array/%sEntries"\n' % key)
    f.write('        android:entryValues="@array/%sValues"\n' % key)

  def WriteAppRestriction(policy):
    policy_name = policy.name
    WriteRestrictionCommon(policy_name)

    if policy.items is not None:
      WriteItemsDefinition(policy_name)

    f.write('        android:restrictionType="%s"/>' % policy.restriction_type)
    f.write('\n\n')

  # _WriteAppRestrictions body
  f.write('<restrictions xmlns:android="'
          'http://schemas.android.com/apk/res/android">\n\n')
  for policy in policies:
    if (policy.is_supported and policy.restriction_type != 'invalid' and
        not policy.is_deprecated and not policy.is_future):
      WriteAppRestriction(policy)
  f.write('</restrictions>')


if __name__ == '__main__':
  sys.exit(main())

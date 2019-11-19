#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Checks a policy_templates.json file for conformity to its syntax specification.
'''

import json
import optparse
import os
import re
import sys
from schema_validator import SchemaValidator

LEADING_WHITESPACE = re.compile('^([ \t]*)')
TRAILING_WHITESPACE = re.compile('.*?([ \t]+)$')
# Matches all non-empty strings that contain no whitespaces.
NO_WHITESPACE = re.compile('[^\s]+$')

# Convert a 'type' to the schema types it may be converted to.
# The 'dict' type represents structured JSON data, and can be converted
# to an 'object' or an 'array'.
TYPE_TO_SCHEMA = {
    'int': ['integer'],
    'list': ['array'],
    'dict': ['object', 'array'],
    'main': ['boolean'],
    'string': ['string'],
    'int-enum': ['integer'],
    'string-enum': ['string'],
    'string-enum-list': ['array'],
    'external': ['object'],
}

# List of boolean policies that have been introduced with negative polarity in
# the past and should not trigger the negative polarity check.
LEGACY_INVERTED_POLARITY_WHITELIST = [
    'DeveloperToolsDisabled',
    'DeviceAutoUpdateDisabled',
    'Disable3DAPIs',
    'DisableAuthNegotiateCnameLookup',
    'DisablePluginFinder',
    'DisablePrintPreview',
    'DisableSafeBrowsingProceedAnyway',
    'DisableScreenshots',
    'DisableSpdy',
    'DisableSSLRecordSplitting',
    'DriveDisabled',
    'DriveDisabledOverCellular',
    'ExternalStorageDisabled',
    'SavingBrowserHistoryDisabled',
    'SyncDisabled',
]

# List of policies where the 'string' part of the schema is actually a JSON
# string which has its own schema.
LEGACY_EMBEDDED_JSON_WHITELIST = [
    'ArcPolicy',
    'AutoSelectCertificateForUrls',
    'DefaultPrinterSelection',
    'DeviceAppPack',
    'DeviceLoginScreenAutoSelectCertificateForUrls',
    'DeviceOpenNetworkConfiguration',
    'NativePrinters',
    'OpenNetworkConfiguration',
    'RemoteAccessHostDebugOverridePolicies',
    # NOTE: Do not add any new policies to this list! Do not store policies with
    # complex schemas using stringified JSON - instead, store them as dicts.
]

# 100 MiB upper limit on the total device policy external data max size limits
# due to the security reasons.
# You can increase this limit if you're introducing new external data type
# device policy, but be aware that too heavy policies could result in user
# profiles not having enough space on the device.
TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE = 1024 * 1024 * 100

# Each policy must have a description message shorter than 4096 characters in
# all its translations (ADM format limitation). However, translations of the
# description might exceed this limit, so a lower limit of is used instead.
POLICY_DESCRIPTION_LENGTH_SOFT_LIMIT = 3500


class PolicyTemplateChecker(object):

  def __init__(self):
    self.error_count = 0
    self.warning_count = 0
    self.num_policies = 0
    self.num_groups = 0
    self.num_policies_in_groups = 0
    self.options = None
    self.features = []
    self.schema_validator = SchemaValidator()
    self.has_schema_error = False

  def _Error(self,
             message,
             parent_element=None,
             identifier=None,
             offending_snippet=None):
    self.error_count += 1
    error = ''
    if identifier is not None and parent_element is not None:
      error += 'In %s %s: ' % (parent_element, identifier)
    print error + 'Error: ' + message
    if offending_snippet is not None:
      print '  Offending:', json.dumps(offending_snippet, indent=2)

  def _CheckContains(self,
                     container,
                     key,
                     value_type,
                     optional=False,
                     parent_element='policy',
                     container_name=None,
                     identifier=None,
                     offending='__CONTAINER__',
                     regexp_check=None):
    '''
    Checks |container| for presence of |key| with value of type |value_type|.
    If |value_type| is string and |regexp_check| is specified, then an error is
    reported when the value does not match the regular expression object.

    |value_type| can also be a list, if more than one type is supported.

    The other parameters are needed to generate, if applicable, an appropriate
    human-readable error message of the following form:

    In |parent_element| |identifier|:
      (if the key is not present):
      Error: |container_name| must have a |value_type| named |key|.
      Offending snippet: |offending| (if specified; defaults to |container|)
      (if the value does not have the required type):
      Error: Value of |key| must be a |value_type|.
      Offending snippet: |container[key]|

    Returns: |container[key]| if the key is present, None otherwise.
    '''
    if identifier is None:
      try:
        identifier = container.get('name')
      except:
        self._Error('Cannot access container name of "%s".' % container_name)
        return None
    if container_name is None:
      container_name = parent_element
    if offending == '__CONTAINER__':
      offending = container
    if key not in container:
      if optional:
        return
      else:
        self._Error(
            '%s must have a %s "%s".' % (container_name.title(),
                                         value_type.__name__, key),
            container_name, identifier, offending)
      return None
    value = container[key]
    value_types = value_type if isinstance(value_type, list) else [value_type]
    if not any(isinstance(value, type) for type in value_types):
      self._Error(
          'Value of "%s" must be one of [ %s ].' % (key, ', '.join(
              [type.__name__ for type in value_types])), container_name,
          identifier, value)
    if str in value_types and regexp_check and not regexp_check.match(value):
      self._Error(
          'Value of "%s" must match "%s".' % (key, regexp_check.pattern),
          container_name, identifier, value)
    return value

  def _AddPolicyID(self, id, policy_ids, policy, deleted_policy_ids):
    '''
    Adds |id| to |policy_ids|. Generates an error message if the
    |id| exists already; |policy| is needed for this message.
    '''
    if id in policy_ids:
      self._Error('Duplicate id', 'policy', policy.get('name'), id)
    elif id in deleted_policy_ids:
      self._Error('Deleted id', 'policy', policy.get('name'), id)
    else:
      policy_ids.add(id)

  def _CheckPolicyIDs(self, policy_ids, deleted_policy_ids):
    '''
    Checks a set of policy_ids to make sure it contains a continuous range
    of entries (i.e. no holes).
    Holes would not be a technical problem, but we want to ensure that nobody
    accidentally omits IDs.
    '''
    policy_count = len(policy_ids) + len(deleted_policy_ids)
    for i in range(policy_count):
      if (i + 1) not in policy_ids and (i + 1) not in deleted_policy_ids:
        self._Error('No policy with id: %s' % (i + 1))

  def _CheckHighestId(self, policy_ids, highest_id):
    '''
    Checks that the 'highest_id_currently_used' value is actually set to the
    highest id in use by any policy.
    '''
    highest_id_in_policies = max(policy_ids)
    if highest_id != highest_id_in_policies:
      self._Error(("\'highest_id_currently_used\' must be set to the highest"
                   "policy id in use, which is currently %s (vs %s).") %
                  (highest_id_in_policies, highest_id))

  def _CheckPolicySchema(self, policy, policy_type):
    '''Checks that the 'schema' field matches the 'type' field.'''
    self.has_schema_error = False
    schema = self._CheckContains(policy, 'schema', dict)
    if schema:
      schema_type = self._CheckContains(schema, 'type', str)
      if schema_type not in TYPE_TO_SCHEMA[policy_type]:
        self._Error('Schema type must match the existing type for policy %s' %
                    policy.get('name'))
      if not self.schema_validator.ValidateSchema(schema):
        self._Error('Schema is invalid for policy %s' % policy.get('name'))
        self.has_schema_error = True

    if 'validation_schema' in policy:
      validation_schema = policy.get('validation_schema')
      if not self.schema_validator.ValidateSchema(validation_schema):
        self._Error(
            'Validation schema is invalid for policy %s' % policy.get('name'))
        self.has_schema_error = True

    # Checks that boolean policies are not negated (which makes them harder to
    # reason about).
    if (policy_type == 'main' and 'disable' in policy.get('name').lower() and
        policy.get('name') not in LEGACY_INVERTED_POLARITY_WHITELIST):
      self._Error(('Boolean policy %s uses negative polarity, please make ' +
                   'new boolean policies follow the XYZEnabled pattern. ' +
                   'See also http://crbug.com/85687') % policy.get('name'))

    # Checks that the policy doesn't have a validation_schema - the whole
    # schema should be defined in 'schema'- unless whitelisted as legacy.
    if ('validation_schema' in policy and
        policy.get('name') not in LEGACY_EMBEDDED_JSON_WHITELIST):
      self._Error(('"validation_schema" is defined for new policy %s - ' +
                   'entire schema data should be contained in "schema"') %
                  policy.get('name'))

    # Try to make sure that any policy with a complex schema is storing it as
    # a 'dict', not embedding it inside JSON strings - unless whitelisted.
    if (self._AppearsToContainEmbeddedJson(policy.get('example_value')) and
        policy.get('name') not in LEGACY_EMBEDDED_JSON_WHITELIST):
      self._Error(('Example value for new policy %s looks like JSON. Do ' +
                   'not store complex data as stringified JSON - instead, ' +
                   'store it in a dict and define it in "schema".') %
                  policy.get('name'))

  def _CheckTotalDevicePolicyExternalDataMaxSize(self, policy_definitions):
    total_device_policy_external_data_max_size = 0
    for policy in policy_definitions:
      if (policy.get('device_only', False) and
          self._CheckContains(policy, 'type', str) == 'external'):
        total_device_policy_external_data_max_size += self._CheckContains(
            policy, 'max_size', int)
    if (total_device_policy_external_data_max_size >
        TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE):
      self._Error(
          ('Total sum of device policy external data maximum size limits ' +
           'should not exceed %d bytes, current sum is %d bytes.') %
          (TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE,
           total_device_policy_external_data_max_size))

  # Returns True if the example value for a policy seems to contain JSON
  # embedded inside a string. Simply checks if strings start with '{', so it
  # doesn't flag numbers (which are valid JSON) but it does flag both JSON
  # objects and python objects (regardless of the type of quotes used).
  def _AppearsToContainEmbeddedJson(self, example_value):
    if isinstance(example_value, str):
      return example_value.strip().startswith('{')
    elif isinstance(example_value, list):
      return any(self._AppearsToContainEmbeddedJson(v) for v in example_value)
    elif isinstance(example_value, dict):
      return any(
          self._AppearsToContainEmbeddedJson(v)
          for v in example_value.itervalues())

  # Checks that there are no duplicate proto paths in device_policy_proto_map.
  def _CheckDevicePolicyProtoMappingUniqueness(self, device_policy_proto_map,
                                               legacy_device_policy_proto_map):
    # Check that device_policy_proto_map does not have duplicate values.
    proto_paths = set()
    for proto_path in device_policy_proto_map.itervalues():
      if proto_path in proto_paths:
        self._Error(
            "Duplicate proto path '%s' in device_policy_proto_map. Did you set "
            "the right path for your device policy?" % proto_path)
      proto_paths.add(proto_path)

    # Check that legacy_device_policy_proto_map only contains pairs
    # [policy_name, proto_path] and does not have duplicate proto_paths.
    for policy_and_path in legacy_device_policy_proto_map:
      if len(policy_and_path) != 2 or not isinstance(
          policy_and_path[0], str) or not isinstance(policy_and_path[1], str):
        self._Error(
            "Every entry in legacy_device_policy_proto_map must be an array of "
            "two strings, but found '%s'" % policy_and_path)
      if policy_and_path[1] != '' and policy_and_path[1] in proto_paths:
        self._Error(
            "Duplicate proto path '%s' in legacy_device_policy_proto_map. Did "
            "you set the right path for your device policy?" %
            policy_and_path[1])
      proto_paths.add(policy_and_path[1])

  # If 'device only' field is true, the policy must be mapped to its proto
  # field in device_policy_proto_map.json.
  def _CheckDevicePolicyProtoMappingDeviceOnly(
      self, policy, device_policy_proto_map, legacy_device_policy_proto_map):
    if not policy.get('device_only', False):
      return

    name = policy.get('name')
    if not name in device_policy_proto_map and not any(
        name == policy_and_path[0]
        for policy_and_path in legacy_device_policy_proto_map):
      self._Error(
          "Please add '%s' to device_policy_proto_map and map it to "
          "the corresponding field in chrome_device_policy.proto." % name)
      return

  # Performs a quick check whether all fields in |device_policy_proto_map| are
  # actually present in the device policy proto at |device_policy_proto_path|.
  # Note that this presubmit check can't compile the proto to pb2.py easily (or
  # can it?).
  def _CheckDevicePolicyProtoMappingExistence(self, device_policy_proto_map,
                                              device_policy_proto_path):
    with open(device_policy_proto_path, 'r') as file:
      device_policy_proto = file.read()

    for policy, proto_path in device_policy_proto_map.items():
      fields = proto_path.split(".")
      for field in fields:
        if field not in device_policy_proto:
          self._Error("Bad device_policy_proto_map for policy '%s': "
                      "Field '%s' not present in device policy proto." %
                      (policy, field))

  def _CheckPolicy(self, policy, is_in_group, policy_ids, deleted_policy_ids):
    if not isinstance(policy, dict):
      self._Error('Each policy must be a dictionary.', 'policy', None, policy)
      return

    # There should not be any unknown keys in |policy|.
    for key in policy:
      if key not in ('name', 'owners', 'type', 'caption', 'desc', 'device_only',
                     'supported_on', 'label', 'policies', 'items',
                     'example_value', 'features', 'deprecated', 'future', 'id',
                     'schema', 'validation_schema', 'description_schema',
                     'url_schema', 'max_size', 'tags',
                     'default_for_enterprise_users',
                     'default_for_managed_devices_doc_only', 'arc_support',
                     'supported_chrome_os_management'):
        self.warning_count += 1
        print('In policy %s: Warning: Unknown key: %s' % (policy.get('name'),
                                                          key))

    # Each policy must have a name.
    self._CheckContains(policy, 'name', str, regexp_check=NO_WHITESPACE)

    # Each policy must have a type.
    policy_types = ('group', 'main', 'string', 'int', 'list', 'int-enum',
                    'string-enum', 'string-enum-list', 'dict', 'external')
    policy_type = self._CheckContains(policy, 'type', str)
    if policy_type not in policy_types:
      self._Error('Policy type must be one of: ' + ', '.join(policy_types),
                  'policy', policy.get('name'), policy_type)
      return  # Can't continue for unsupported type.

    # Each policy must have a caption message.
    self._CheckContains(policy, 'caption', str)

    # Each policy's description should be within the limit.
    desc = self._CheckContains(policy, 'desc', str)
    if len(desc.decode("UTF-8")) > POLICY_DESCRIPTION_LENGTH_SOFT_LIMIT:
      self._Error(
          'Length of description is more than %d characters, which might '
          'exceed the limit of 4096 characters in one of its '
          'translations. If there is no alternative to reducing the length '
          'of the description, it is recommended to add a page under %s '
          'instead and provide a link to it.' %
          (POLICY_DESCRIPTION_LENGTH_SOFT_LIMIT,
           'https://www.chromium.org/administrators'), 'policy',
          policy.get('name'))

    # If 'label' is present, it must be a string.
    self._CheckContains(policy, 'label', str, True)

    # If 'deprecated' is present, it must be a bool.
    self._CheckContains(policy, 'deprecated', bool, True)

    # If 'future' is present, it must be a bool.
    self._CheckContains(policy, 'future', bool, True)

    # If 'arc_support' is present, it must be a string.
    self._CheckContains(policy, 'arc_support', str, True)

    if policy_type == 'group':
      # Groups must not be nested.
      if is_in_group:
        self._Error('Policy groups must not be nested.', 'policy', policy)

      # Each policy group must have a list of policies.
      policies = self._CheckContains(policy, 'policies', list)

      # Policy list should not be empty
      if isinstance(policies, list) and len(policies) == 0:
        self._Error('Policy list should not be empty.', 'policies', None,
                    policy)

      # Groups must not have an |id|.
      if 'id' in policy:
        self._Error('Policies of type "group" must not have an "id" field.',
                    'policy', policy)

      # Statistics.
      self.num_groups += 1

    else:  # policy_type != group
      # Each policy must have a protobuf ID.
      id = self._CheckContains(policy, 'id', int)
      self._AddPolicyID(id, policy_ids, policy, deleted_policy_ids)

      # Each policy must have an owner.
      # TODO(pastarmovj): Verify that each owner is either an OWNERS file or an
      # email of a committer.
      self._CheckContains(policy, 'owners', list)

      # Each policy must have a tag list.
      self._CheckContains(policy, 'tags', list)

      # 'schema' is the new 'type'.
      # TODO(joaodasilva): remove the 'type' checks once 'schema' is used
      # everywhere.
      self._CheckPolicySchema(policy, policy_type)

      # Each policy must have a supported_on list.
      supported_on = self._CheckContains(policy, 'supported_on', list)
      if supported_on is not None:
        for s in supported_on:
          if not isinstance(s, str):
            self._Error('Entries in "supported_on" must be strings.', 'policy',
                        policy, supported_on)

      # Each policy must have a 'features' dict.
      features = self._CheckContains(policy, 'features', dict)

      # All the features must have a documenting message.
      if features:
        for feature in features:
          if not feature in self.features:
            self._Error(
                'Unknown feature "%s". Known features must have a '
                'documentation string in the messages dictionary.' % feature,
                'policy', policy.get('name', policy))

      # All user policies must have a per_profile feature flag.
      if (not policy.get('device_only', False) and
          not policy.get('deprecated', False) and
          not filter(re.compile('^chrome_frame:.*').match, supported_on)):
        self._CheckContains(
            features,
            'per_profile',
            bool,
            container_name='features',
            identifier=policy.get('name'))

      # If 'device only' policy is on, feature 'per_profile' shouldn't exist.
      if (policy.get('device_only', False) and
          features.get('per_profile', False)):
        self._Error('per_profile attribute should not be set '
                    'for policies with device_only=True')

      # If 'device only' policy is on, 'default_for_enterprise_users' shouldn't
      # exist.
      if (policy.get('device_only', False) and
          'default_for_enterprise_users' in policy):
        self._Error('default_for_enteprise_users should not be set '
                    'for policies with device_only=True. Please use '
                    'default_for_managed_devices_doc_only to document a'
                    'differing default value for enrolled devices. Please note '
                    'that default_for_managed_devices_doc_only is for '
                    'documentation only - it has no side effects, so you will '
                    ' still have to implement the enrollment-dependent default '
                    'value handling yourself in all places where the device '
                    'policy proto is evaluated. This will probably include '
                    'device_policy_decoder_chromeos.cc for chrome, but could '
                    'also have to done in other components if they read the '
                    'proto directly. Details: crbug.com/809653')

      if (not policy.get('device_only', False) and
          'default_for_managed_devices_doc_only' in policy):
        self._Error('default_for_managed_devices_doc_only should only be used '
                    'with policies that have device_only=True.')

      # All policies must declare whether they allow changes at runtime.
      self._CheckContains(
          features,
          'dynamic_refresh',
          bool,
          container_name='features',
          identifier=policy.get('name'))

      # Chrome OS policies may have a non-empty supported_chrome_os_management
      # list with either 'active_directory' or 'google_cloud' or both.
      supported_chrome_os_management = self._CheckContains(
          policy, 'supported_chrome_os_management', list, True)
      if supported_chrome_os_management is not None:
        # Must be on Chrome OS.
        if (supported_on is not None and
            not any('chrome_os:' in str for str in supported_on)):
          self._Error(
              '"supported_chrome_os_management" is only supported on '
              'Chrome OS', 'policy', policy, supported_on)
        # Must be non-empty.
        if len(supported_chrome_os_management) == 0:
          self._Error('"supported_chrome_os_management" must be non-empty',
                      'policy', policy)
        # Must be either 'active_directory' or 'google_cloud'.
        if (any(str != 'google_cloud' and str != 'active_directory'
                for str in supported_chrome_os_management)):
          self._Error(
              'Values in "supported_chrome_os_management" must be '
              'either "active_directory" or "google_cloud"', 'policy', policy,
              supported_chrome_os_management)

      # Each policy must have an 'example_value' of appropriate type.
      if policy_type == 'main':
        value_type = item_type = bool
      elif policy_type in ('string', 'string-enum'):
        value_type = item_type = str
      elif policy_type in ('int', 'int-enum'):
        value_type = item_type = int
      elif policy_type in ('list', 'string-enum-list'):
        value_type = list
        item_type = str
      elif policy_type == 'external':
        value_type = item_type = dict
      elif policy_type == 'dict':
        value_type = item_type = [dict, list]
      else:
        raise NotImplementedError('Unimplemented policy type: %s' % policy_type)
      self._CheckContains(policy, 'example_value', value_type)

      # Verify that the example complies with the schema and that all properties
      # are used at least once, so the examples are as useful as possible for
      # admins.
      schema = policy.get('schema')
      example = policy.get('example_value')
      if not self.has_schema_error:
        if not self.schema_validator.ValidateValue(
            schema, example, enforce_use_entire_schema=True):
          self._Error(('Example for policy %s does not comply to the policy\'s '
                       'schema or does not use all properties at least once.') %
                      policy.get('name'))
        if 'validation_schema' in policy and 'description_schema' in policy:
          self._Error(('validation_schema and description_schema both defined '
                       'for policy %s.') % policy.get('name'))
        secondary_schema = policy.get('validation_schema',
                                      policy.get('description_schema'))
        if secondary_schema:
          real_example = {}
          if policy_type == 'string':
            real_example = json.loads(example)
          elif policy_type == 'list':
            real_example = [json.loads(entry) for entry in example]
          else:
            self._Error('Unsupported type for legacy embedded json policy.')
          if not self.schema_validator.ValidateValue(
              secondary_schema, real_example, enforce_use_entire_schema=True):
            self._Error(('Example for policy %s does not comply to the ' +
                         'policy\'s validation_schema') % policy.get('name'))

      # Statistics.
      self.num_policies += 1
      if is_in_group:
        self.num_policies_in_groups += 1

    if policy_type in ('int-enum', 'string-enum', 'string-enum-list'):
      # Enums must contain a list of items.
      items = self._CheckContains(policy, 'items', list)
      if items is not None:
        if len(items) < 1:
          self._Error('"items" must not be empty.', 'policy', policy, items)
        for item in items:
          # Each item must have a name.
          # Note: |policy.get('name')| is used instead of |policy['name']|
          # because it returns None rather than failing when no key called
          # 'name' exists.
          self._CheckContains(
              item,
              'name',
              str,
              container_name='item',
              identifier=policy.get('name'),
              regexp_check=NO_WHITESPACE)

          # Each item must have a value of the correct type.
          self._CheckContains(
              item,
              'value',
              item_type,
              container_name='item',
              identifier=policy.get('name'))

          # Each item must have a caption.
          self._CheckContains(
              item,
              'caption',
              str,
              container_name='item',
              identifier=policy.get('name'))

    if policy_type == 'external':
      # Each policy referencing external data must specify a maximum data size.
      self._CheckContains(policy, 'max_size', int)

  def _CheckMessage(self, key, value):
    # |key| must be a string, |value| a dict.
    if not isinstance(key, str):
      self._Error('Each message key must be a string.', 'message', key, key)
      return

    if not isinstance(value, dict):
      self._Error('Each message must be a dictionary.', 'message', key, value)
      return

    # Each message must have a desc.
    self._CheckContains(
        value, 'desc', str, parent_element='message', identifier=key)

    # Each message must have a text.
    self._CheckContains(
        value, 'text', str, parent_element='message', identifier=key)

    # There should not be any unknown keys in |value|.
    for vkey in value:
      if vkey not in ('desc', 'text'):
        self.warning_count += 1
        print 'In message %s: Warning: Unknown key: %s' % (key, vkey)

  def _LeadingWhitespace(self, line):
    match = LEADING_WHITESPACE.match(line)
    if match:
      return match.group(1)
    return ''

  def _TrailingWhitespace(self, line):
    match = TRAILING_WHITESPACE.match(line)
    if match:
      return match.group(1)
    return ''

  def _LineError(self, message, line_number):
    self.error_count += 1
    print 'In line %d: Error: %s' % (line_number, message)

  def _LineWarning(self, message, line_number):
    self.warning_count += 1
    print('In line %d: Warning: Automatically fixing formatting: %s' %
          (line_number, message))

  def _CheckFormat(self, filename):
    if self.options.fix:
      fixed_lines = []
    # Three quotes open and close multiple lines strings. Odd means currently
    # inside a multiple line strings. We don't change indentation for those
    # strings. It changes hash of the string and grit can't find translation in
    # the file.
    three_quotes_cnt = 0
    with open(filename) as f:
      indent = 0
      line_number = 0
      for line in f:
        line_number += 1
        line = line.rstrip('\n')
        # Check for trailing whitespace.
        trailing_whitespace = self._TrailingWhitespace(line)
        if len(trailing_whitespace) > 0:
          if self.options.fix:
            line = line.rstrip()
            self._LineWarning('Trailing whitespace.', line_number)
          else:
            self._LineError('Trailing whitespace.', line_number)
        if self.options.fix:
          if len(line) == 0:
            fixed_lines += ['\n']
            continue
        else:
          if line == trailing_whitespace:
            # This also catches the case of an empty line.
            continue
        # Check for correct amount of leading whitespace.
        leading_whitespace = self._LeadingWhitespace(line)
        if leading_whitespace.count('\t') > 0:
          if self.options.fix:
            leading_whitespace = leading_whitespace.replace('\t', '  ')
            line = leading_whitespace + line.lstrip()
            self._LineWarning('Tab character found.', line_number)
          else:
            self._LineError('Tab character found.', line_number)
        if line[len(leading_whitespace)] in (']', '}'):
          indent -= 2
        # Ignore 0-indented comments and multiple string literals.
        if line[0] != '#' and three_quotes_cnt % 2 == 0:
          if len(leading_whitespace) != indent:
            if self.options.fix:
              line = ' ' * indent + line.lstrip()
              self._LineWarning(
                  'Indentation should be ' + str(indent) + ' spaces.',
                  line_number)
            else:
              self._LineError(
                  'Bad indentation. Should be ' + str(indent) + ' spaces.',
                  line_number)
        three_quotes_cnt += line.count("'''")
        if line[-1] in ('[', '{'):
          indent += 2
        if self.options.fix:
          fixed_lines.append(line + '\n')

    assert three_quotes_cnt % 2 == 0
    # If --fix is specified: backup the file (deleting any existing backup),
    # then write the fixed version with the old filename.
    if self.options.fix:
      if self.options.backup:
        backupfilename = filename + '.bak'
        if os.path.exists(backupfilename):
          os.remove(backupfilename)
        os.rename(filename, backupfilename)
      with open(filename, 'w') as f:
        f.writelines(fixed_lines)

  def _ValidatePolicyAtomicGroups(self, atomic_groups, max_id):
    ids = [x['id'] for x in atomic_groups]
    actual_highest_id = max(ids)
    if actual_highest_id != max_id:
      self._Error(
          ("\'highest_atomic_group_id_currently_used\' must be set to the "
           "highest atomic group id in use, which is currently %s (vs %s).") %
          (actual_highest_id, max_id))
      return

    ids_set = set()
    for i in range(len(ids)):
      if (ids[i] in ids_set):
        self._Error('Duplicate atomic group id %s' % (ids[i]))
        return
      ids_set.add(ids[i])
      if i + 1 != ids[i]:
        self._Error('Missing atomic group id %s' % (i + 1))
        return

  def Main(self, filename, options):
    try:
      with open(filename, "rb") as f:
        data = eval(f.read().decode("UTF-8"))
    except:
      import traceback
      traceback.print_exc(file=sys.stdout)
      self._Error('Invalid Python/JSON syntax.')
      return 1
    if data == None:
      self._Error('Invalid Python/JSON syntax.')
      return 1
    self.options = options

    # First part: check JSON structure.

    # Check (non-policy-specific) message definitions.
    messages = self._CheckContains(
        data,
        'messages',
        dict,
        parent_element=None,
        container_name='The root element',
        offending=None)
    if messages is not None:
      for message in messages:
        self._CheckMessage(message, messages[message])
        if message.startswith('doc_feature_'):
          self.features.append(message[12:])

    # Check policy definitions.
    policy_definitions = self._CheckContains(
        data,
        'policy_definitions',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)
    deleted_policy_ids = self._CheckContains(
        data,
        'deleted_policy_ids',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)
    highest_id = self._CheckContains(
        data,
        'highest_id_currently_used',
        int,
        parent_element=None,
        container_name='The root element',
        offending=None)
    highest_atomic_group_id = self._CheckContains(
        data,
        'highest_atomic_group_id_currently_used',
        int,
        parent_element=None,
        container_name='The root element',
        offending=None)
    device_policy_proto_map = self._CheckContains(
        data,
        'device_policy_proto_map',
        dict,
        parent_element=None,
        container_name='The root element',
        offending=None)
    legacy_device_policy_proto_map = self._CheckContains(
        data,
        'legacy_device_policy_proto_map',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)
    policy_atomic_group_definitions = self._CheckContains(
        data,
        'policy_atomic_group_definitions',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)

    self._ValidatePolicyAtomicGroups(policy_atomic_group_definitions,
                                     highest_atomic_group_id)
    self._CheckDevicePolicyProtoMappingUniqueness(
        device_policy_proto_map, legacy_device_policy_proto_map)
    self._CheckDevicePolicyProtoMappingExistence(
        device_policy_proto_map, options.device_policy_proto_path)

    if policy_definitions is not None:
      policy_ids = set()
      for policy in policy_definitions:
        self._CheckPolicy(policy, False, policy_ids, deleted_policy_ids)
        self._CheckDevicePolicyProtoMappingDeviceOnly(
            policy, device_policy_proto_map, legacy_device_policy_proto_map)
      self._CheckPolicyIDs(policy_ids, deleted_policy_ids)
      if highest_id is not None:
        self._CheckHighestId(policy_ids, highest_id)
      self._CheckTotalDevicePolicyExternalDataMaxSize(policy_definitions)

    # Made it as a dict (policy_name -> True) to reuse _CheckContains.
    policy_names = {
        policy['name']: True
        for policy in policy_definitions
        if policy['type'] != 'group'
    }
    policy_in_groups = set()
    for group in [
        policy for policy in policy_definitions if policy['type'] == 'group'
    ]:
      for policy_name in group['policies']:
        self._CheckContains(
            policy_names,
            policy_name,
            bool,
            parent_element='policy_definitions')
        if policy_name in policy_in_groups:
          self._Error('Policy %s defined in several groups.' % (policy_name))
        else:
          policy_in_groups.add(policy_name)

    policy_in_atomic_groups = set()
    for group in policy_atomic_group_definitions:
      for policy_name in group['policies']:
        self._CheckContains(
            policy_names,
            policy_name,
            bool,
            parent_element='policy_definitions')
        if policy_name in policy_in_atomic_groups:
          self._Error('Policy %s defined in several atomic policy groups.' %
                      (policy_name))
        else:
          policy_in_atomic_groups.add(policy_name)

    # Second part: check formatting.
    self._CheckFormat(filename)

    # Third part: summary and exit.
    print('Finished checking %s. %d errors, %d warnings.' %
          (filename, self.error_count, self.warning_count))
    if self.options.stats:
      if self.num_groups > 0:
        print('%d policies, %d of those in %d groups (containing on '
              'average %.1f policies).' %
              (self.num_policies, self.num_policies_in_groups, self.num_groups,
               (1.0 * self.num_policies_in_groups / self.num_groups)))
      else:
        print self.num_policies, 'policies, 0 policy groups.'
    if self.error_count > 0:
      return 1
    return 0

  def Run(self, argv, filename=None):
    parser = optparse.OptionParser(
        usage='usage: %prog [options] filename',
        description='Syntax check a policy_templates.json file.')
    parser.add_option(
        '--device_policy_proto_path',
        help='[REQUIRED] File path of the device policy proto file.',
        type='string')
    parser.add_option(
        '--fix', action='store_true', help='Automatically fix formatting.')
    parser.add_option(
        '--backup',
        action='store_true',
        help='Create backup of original file (before fixing).')
    parser.add_option(
        '--stats', action='store_true', help='Generate statistics.')
    (options, args) = parser.parse_args(argv)
    if filename is None:
      if len(args) != 2:
        parser.print_help()
        return 1
      filename = args[1]
    if options.device_policy_proto_path is None:
      print('Error: Missing --device_policy_proto_path argument.')
      return 1
    return self.Main(filename, options)


if __name__ == '__main__':
  sys.exit(PolicyTemplateChecker().Run(sys.argv))

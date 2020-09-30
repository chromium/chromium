#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Checks a policy_templates.json file for conformity to its syntax specification.
'''

import argparse
import ast
import json
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
LEGACY_INVERTED_POLARITY_ALLOWLIST = [
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
LEGACY_EMBEDDED_JSON_ALLOWLIST = [
    'ArcPolicy',
    'AutoSelectCertificateForUrls',
    'DefaultPrinterSelection',
    'DeviceAppPack',
    'DeviceLoginScreenAutoSelectCertificateForUrls',
    'DeviceOpenNetworkConfiguration',
    'NativePrinters',
    'Printers',
    'OpenNetworkConfiguration',
    'RemoteAccessHostDebugOverridePolicies',
    # NOTE: Do not add any new policies to this list! Do not store policies with
    # complex schemas using stringified JSON - instead, store them as dicts.
]

# List of policies where not all properties are required to be presented in the
# example value. This could be useful e.g. in case of mutually exclusive fields.
# See crbug.com/1068257 for the details.
OPTIONAL_PROPERTIES_POLICIES_ALLOWLIST = []

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

# Dictionaries that define how the checks can determine if a change to a policy
# value are backwards compatible.

# Defines specific keys in specific types that have custom validation functions
# for checking if a change to the value is a backwards compatible change.
# For instance increasing the 'maxmimum' value for an integer is less
# restrictive than decreasing it.
CUSTOM_VALUE_CHANGE_VALIDATION_PER_TYPE = {
    'integer': {
        'minimum': lambda old_value, new_value: new_value <= old_value,
        'maximum': lambda old_value, new_value: new_value >= old_value
    }
}

# Defines keys per type that can simply be removed in a newer version of a
# policy. For example, removing a 'required' field makes a policy schema less
# restrictive.
# This dictionary allows us to state that the given key can be totally removed
# when checking for a particular type. Or if the key usually represents an
# array of values, it states that entries in the array can be removed. Normally
# no array value can be removed in a policy change if we want to keep it
# backwards compatible.
REMOVABLE_SCHEMA_VALUES_PER_TYPE = {
    'integer': ['minimum', 'maximum'],
    'string': ['pattern'],
    'object': ['required']
}

# Defines keys per type that that can be changed in any way without affecting
# policy compatibility (for example we can change, remove or add a 'description'
# to a policy schema without causings incompatibilities).
MODIFIABLE_SCHEMA_KEYS_PER_TYPE = {
    'integer': ['description', 'sensitiveValue'],
    'string': ['description', 'sensitiveValue'],
    'object': ['description', 'sensitiveValue']
}

# Defines keys per type that themselves define a further dictionary of
# properties each with their own schemas. For example, 'object' types define
# a 'properties' key that list all the possible keys in the object.
KEYS_DEFINING_PROPERTY_DICT_SCHEMAS_PER_TYPE = {
    'object': ['properties', 'patternProperties']
}

# Defines keys per type that themselves define a schema. For example, 'array'
# types define an 'items' key defines the scheme for each item in the array.
KEYS_DEFINING_SCHEMAS_PER_TYPE = {
    'object': ['additionalProperties'],
    'array': ['items']
}

# The list of platforms policy could support.
ALL_SUPPORTED_PLATFORMS = [
    'chrome_frame', 'chrome_os', 'android', 'webview_android', 'ios',
    'chrome.win', 'chrome.win7', 'chrome.linux', 'chrome.mac', 'chrome.*'
]

# The list of platforms that chrome.* represents.
CHROME_STAR_PLATFORMS = ['chrome.win', 'chrome.mac', 'chrome.linux']


# Helper function to determine if a given type defines a key in a dictionary
# that is used to condition certain backwards compatibility checks.
def IsKeyDefinedForTypeInDictionary(type, key, key_per_type_dict):
  return type in key_per_type_dict and key in key_per_type_dict[type]


# Helper function that expand chrome.* in the |platforms| list or dict.
def ExpandChromeStar(platforms):
  if platforms and 'chrome.*' in platforms:
    if isinstance(platforms, list):
      index = platforms.index('chrome.*')
      platforms[index:index + 1] = CHROME_STAR_PLATFORMS
    elif isinstance(platforms, dict):
      value = platforms.pop('chrome.*')
      for chrome_star_platform in CHROME_STAR_PLATFORMS:
        # copy reference here as the value shouldn't be changed.
        platforms[chrome_star_platform] = value
  return platforms


def _GetSupportedVersionPlatformAndRange(supported_on):
  (supported_on_platform, supported_on_versions) = supported_on.split(':')

  (supported_on_from, supported_on_to) = supported_on_versions.split('-')

  return supported_on_platform, (int(supported_on_from) if supported_on_from
                                 else None), (int(supported_on_to)
                                              if supported_on_to else None)


def _PolicyStillSupported(supported_on, current_version):
  for s in supported_on:
    _, _, supported_on_to = _GetSupportedVersionPlatformAndRange(s)

    # If supported_on_to isn't given, this policy is still supported.
    if supported_on_to is None:
      return True

    # If supported_on_to is equal or greater than the current version, it's
    # still supported.
    if current_version <= int(supported_on_to):
      return True

  return False


def MergeDict(*dicts):
  result = {}
  for dictionary in dicts:
    result.update(dictionary)
  return result


class DuplicateKeyVisitor(ast.NodeVisitor):
  def visit_Dict(self, node):
    seen_keys = set()
    for i, node_key in enumerate(node.keys):
      key = ast.literal_eval(node_key)
      if key in seen_keys:
        raise ValueError("Duplicate key '%s' in line %d found." %
                         (key, node.values[i].lineno))
      seen_keys.add(key)

    # Recursively check for all nested objects.
    self.generic_visit(node)


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

  def _Warning(self, message):
    self.warning_count += 1
    print message

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
      self._Error(("'highest_id_currently_used' must be set to the highest"
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
    if (policy_type == 'main' and 'disable' in policy.get('name').lower()
        and policy.get('name') not in LEGACY_INVERTED_POLARITY_ALLOWLIST):
      self._Error(('Boolean policy %s uses negative polarity, please make ' +
                   'new boolean policies follow the XYZEnabled pattern. ' +
                   'See also http://crbug.com/85687') % policy.get('name'))

    # Checks that the policy doesn't have a validation_schema - the whole
    # schema should be defined in 'schema'- unless listed as legacy.
    if ('validation_schema' in policy
        and policy.get('name') not in LEGACY_EMBEDDED_JSON_ALLOWLIST):
      self._Error(('"validation_schema" is defined for new policy %s - ' +
                   'entire schema data should be contained in "schema"') %
                  policy.get('name'))

    # Try to make sure that any policy with a complex schema is storing it as
    # a 'dict', not embedding it inside JSON strings - unless listed as legacy.
    if (self._AppearsToContainEmbeddedJson(policy.get('example_value'))
        and policy.get('name') not in LEGACY_EMBEDDED_JSON_ALLOWLIST):
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

  def _CheckPolicy(self, policy, is_in_group, policy_ids, deleted_policy_ids,
                   current_version):
    if not isinstance(policy, dict):
      self._Error('Each policy must be a dictionary.', 'policy', None, policy)
      return

    # There should not be any unknown keys in |policy|.
    for key in policy:
      if key not in (
          'name',
          'owners',
          'type',
          'caption',
          'desc',
          'device_only',
          'supported_on',
          'label',
          'policies',
          'items',
          'example_value',
          'features',
          'deprecated',
          'future',
          'future_on',
          'id',
          'schema',
          'validation_schema',
          'description_schema',
          'url_schema',
          'max_size',
          'tags',
          'default_for_enterprise_users',
          'default_for_managed_devices_doc_only',
          'arc_support',
          'supported_chrome_os_management',
      ):
        self._Warning('In policy %s: Warning: Unknown key: %s' %
                      (policy.get('name'), key))

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
    is_future = self._CheckContains(policy, 'future', bool, True)

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
      supported_on = self._CheckContains(policy,
                                         'supported_on',
                                         list,
                                         optional=True)
      supported_platforms = []
      if supported_on:
        for s in supported_on:
          (
              supported_on_platform,
              supported_on_from,
              supported_on_to,
          ) = _GetSupportedVersionPlatformAndRange(s)

          supported_platforms.append(supported_on_platform)
          if not isinstance(supported_on_platform,
                            str) or not supported_on_platform:
            self._Error(
                'Entries in "supported_on" must have a valid target before the '
                '":".', 'policy', policy, supported_on)
          elif not isinstance(supported_on_from, int):
            self._Error(
                'Entries in "supported_on" must have a valid starting '
                'supported version after the ":".', 'policy', policy,
                supported_on)
          elif isinstance(supported_on_to,
                          int) and supported_on_to < supported_on_from:
            self._Error(
                'Entries in "supported_on" that have an ending '
                'supported version must have a version larger than the '
                'starting supported version.', 'policy', policy, supported_on)

        if (not _PolicyStillSupported(supported_on, current_version)
            and not policy.get('deprecated', False)):
          self._Error(
              'Policy %s is marked as no longer supported (%s), but isn\'t '
              'marked as deprecated. Unsupported policies must be marked as '
              '"deprecated": True' % (policy.get('name'), supported_on))

      supported_platforms = ExpandChromeStar(supported_platforms)
      future_on = ExpandChromeStar(
          self._CheckContains(policy, 'future_on', list, optional=True))

      self._CheckPlatform(supported_platforms, 'supported_on',
                          policy.get('name'))
      self._CheckPlatform(future_on, 'future_on', policy.get('name'))

      if not supported_platforms and not future_on:
        self._Error(
            'The policy needs to be supported now or in the future on at '
            'least one platform.', 'policy', policy.get('name'))

      if supported_on == []:
        self._Warning("Policy %s: supported_on' is empty." %
                      (policy.get('name')))

      if future_on == []:
        self._Warning("Policy %s: 'future_on' is empty." % (policy.get('name')))

      if future_on is not None and is_future is not None:
        self._Error(
            "Tag 'future' has been deprecated, please use 'future_on' instead.",
            'policy', policy.get('name'))

      if future_on:
        for platform in set(supported_platforms).intersection(future_on):
          self._Error(
              "Platform %s is marked as 'supported_on' and 'future_on'. Only "
              "put released platform in 'supported_on' field" % (platform),
              'policy', policy.get('name'))


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
      if (not policy.get('device_only', False)
          and not policy.get('deprecated', False)
          and not 'chrome_frame' in supported_platforms):
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

      # 'cloud_only' feature must be an optional boolean flag.
      cloud_only = self._CheckContains(
          features,
          'cloud_only',
          bool,
          optional=True,
          container_name='features')

      # 'platform_only' feature must be an optional boolean flag.
      platform_only = self._CheckContains(
          features,
          'platform_only',
          bool,
          optional=True,
          container_name='features')

      # 'internal_only' feature must be an optional boolean flag.
      platform_only = self._CheckContains(features,
                                          'internal_only',
                                          bool,
                                          optional=True,
                                          container_name='features')

      # 'private' feature must be an optional boolean flag.
      is_unlisted = self._CheckContains(features,
                                        'unlisted',
                                        bool,
                                        optional=True,
                                        container_name='features')

      if cloud_only and platform_only:
        self._Error(
            'cloud_only and platfrom_only must not be true at the same '
            'time.', 'policy', policy.get('name'))

      if is_unlisted and not cloud_only:
        self._Error('unlisted can only be used by cloud_only policy.', 'policy',
                    policy.get('name'))


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
      enforce_use_entire_schema = policy.get(
          'name') not in OPTIONAL_PROPERTIES_POLICIES_ALLOWLIST
      if not self.has_schema_error:
        if not self.schema_validator.ValidateValue(schema, example,
                                                   enforce_use_entire_schema):
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

  def _CheckPlatform(self, platforms, field_name, policy_name):
    ''' Verifies the |platforms| list. Records any error with |field_name| and
        |policy_name|.  '''
    if not platforms:
      return

    duplicated = set()
    for platform in platforms:
      if platform not in ALL_SUPPORTED_PLATFORMS:
        self._Error(
            'Platform %s is not supported in %s. Valid platforms are %s.' %
            (platform, field_name, ', '.join(ALL_SUPPORTED_PLATFORMS)),
            'policy', policy_name)
      if platform in duplicated:
        self._Error(
            'platform %s appears more than once in %s.' %
            (platform, field_name), 'policy', policy_name)
      duplicated.add(platform)

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
        self._Warning('In message %s: Warning: Unknown key: %s' % (key, vkey))

  def _GetReleasedPlatforms(self, policy, current_version):
    '''
    Returns a dictionary that contains released platforms and their released
    version. Returns empty dictionary if policy is None or policy.future is
    True.

    Args:
      policy: A dictionary contains all policy data from policy_templates.json.
      current_version: A integer represents the current major milestone.

    Returns:
      released_platforms: A dictionary contains all platforms that have been
                          released to stable and their released version.
      rolling_out_platform: A dictionary contains all platforms that have been
                            released but haven't reached stable.
      Example:
      {
        'chrome.win' : 10,
        'chrome_os': '10,
      }, {
        'chrome.mac': 15,
      }
    '''

    released_platforms = {}
    rolling_out_platform = {}
    if not policy or policy.get('future', False):
      return released_platforms, rolling_out_platform

    for supported_on in policy.get('supported_on', []):
      supported_platform, supported_from, _ = \
              _GetSupportedVersionPlatformAndRange(supported_on)
      if supported_from < current_version - 1:
        released_platforms[supported_platform] = supported_from
      else:
        rolling_out_platform[supported_platform] = supported_from

    released_platforms = ExpandChromeStar(released_platforms)
    rolling_out_platform = ExpandChromeStar(rolling_out_platform)

    return released_platforms, rolling_out_platform

  def _CheckSingleSchemaValueIsCompatible(
      self, old_schema_value, new_schema_value, custom_value_validation):
    '''
    Checks if a |new_schema_value| in a schema is compatible with an
    |old_schema_value| in a schema. The check will either use the provided
    |custom_value_validation| if any or do a normal equality comparison.
    '''
    return (custom_value_validation == None and
            old_schema_value == new_schema_value) or (
                custom_value_validation != None and
                custom_value_validation(old_schema_value, new_schema_value))

  def _CheckSchemaValueIsCompatible(self, schema_key_path, old_schema_value,
                                    new_schema_value, only_removals_allowed,
                                    custom_value_validation):
    '''
    Checks if two leaf schema values defined by |old_schema_value| and
    |new_schema_value| are compatible with each other given certain conditions
    concerning removal (|only_removals_allowed|) and also for custom
    compatibility validation (|custom_value_validation|). The leaf schema should
    never be a dictionary type.

    |schema_key_path|: Used for error reporting, this is the current path in the
      policy schema that we are processing represented as a list of paths.
    |old_schema_value|: The value of the schema property in the original policy
      templates file.
    |new_schema_value|: The value of the schema property in the modified policy
      templates file.
    |only_removals_allowed|: Specifies whether the schema value can be removed
      in the modified policy templates file. For list type schema values, this
      flag will also allow removing some entries in the list while keeping other
      parts.
    |custom_value_validation|: Custom validation function used to compare the
      old and new values to see if they are compatible. If None is provided then
      an equality comparison is used.
    '''
    current_schema_key = '/'.join(schema_key_path)

    # If there is no new value but an old one exists, generally this is
    # considered an incompatibility and should be reported unless removals are
    # allowed for this value.
    if (new_schema_value == None):
      if not only_removals_allowed:
        self._Error(
            'Value in policy schema path \'%s\' was removed in new schema '
            'value.' % (current_schema_key))
      return

    # Both old and new values must be of the same type.
    if type(old_schema_value) != type(new_schema_value):
      self._Error(
          'Value in policy schema path \'%s\' is of type \'%s\' but value in '
          'schema is of type \'%s\'.' % (current_schema_key,
                                         type(old_schema_value).__name__,
                                         type(new_schema_value).__name__))

    # We are checking a leaf schema key and do not expect to ever get a
    # dictionary value at this level.
    if (type(old_schema_value) is dict):
      self._Error(
          'Value in policy schema path \'%s\' had an unexpected type: \'%s\'.' %
          (current_schema_key, type(old_schema_value).__name__))
    # We have a list type schema value. In general additions to the list are
    # allowed (e.g. adding a new enum value) but removals from the lists are
    # not allowed. Also additions to the list must only occur at the END of the
    # old list and not in the middle.
    elif (type(old_schema_value) is list):
      # If only removal from the list is allowed check that there are no new
      # values and that only old values are removed. Since we are enforcing
      # strict ordering we can check the lists sequentially for this condition.
      if only_removals_allowed:
        j = 0
        i = 0

        # For every old value, check that it either exists in the new value in
        # the same order or was removed. This loop only iterates sequentially
        # on both lists.
        while i < len(old_schema_value) and j < len(new_schema_value):
          # Keep looking in the old value until we find a matching new_value at
          # our current position in the list or until we reach the end of the
          # old values.
          while not self._CheckSingleSchemaValueIsCompatible(
              old_schema_value[i], new_schema_value[j],
              custom_value_validation):
            i += 1
            if i >= len(old_schema_value):
              break

          # Here either we've found the matching old value so that we can say
          # the new value matches and move to the next new value (j += 1) and
          # the next old value (i += 1) to check, or we have exhausted the old
          # value list and can exit the loop.
          if i < len(old_schema_value):
            j += 1
            i += 1
        # Everything we have not processed in the new value list is in error
        # because only allow removal in this list.
        while j < len(new_schema_value):
          self._Error(
              'Value \'%s\' in policy schema path \'%s/[%s]\' was added which '
              'is not allowed.' % (str(new_schema_value[j]), current_schema_key,
                                   j))
          j += 1
      else:
        # If removals are not allowed we should be able to add to the list, but
        # only at the end. We only need to check that all the old values appear
        # in the same order in the new value as in the old value. Everything
        # added after the end of the old value list is allowed.
        # If the new value list is shorter than the old value list we will end
        # up with calls to _CheckSchemaValueIsCompatible where
        # new_schema_value == None and this will raise an error on the first
        # check in the function.
        for i in range(len(old_schema_value)):
          self._CheckSchemaValueIsCompatible(
              schema_key_path + ['[' + str(i) + ']'], old_schema_value[i],
              new_schema_value[i] if len(new_schema_value) > i else None,
              only_removals_allowed, custom_value_validation)
    # For non list values, we compare the two values against each other with
    # the custom_value_validation or standard equality comparisons.
    elif not self._CheckSingleSchemaValueIsCompatible(
        old_schema_value, new_schema_value, custom_value_validation):
      self._Error(
          'Value in policy schema path \'%s\' was changed from \'%s\' to '
          '\'%s\' which is not allowed.' %
          (current_schema_key, str(old_schema_value), str(new_schema_value)))

  def _CheckSchemasAreCompatible(self, schema_key_path, old_schema, new_schema):
    current_schema_key = '/'.join(schema_key_path)
    '''
    Checks if two given schemas are compatible with each other.

    This function will raise errors if it finds any incompatibilities between
    the |old_schema| and |new_schema|.

    |schema_key_path|: Used for error reporting, this is the current path in the
      policy schema that we are processing represented as a list of paths.
    |old_schema|: The full contents of the schema as found in the original
      policy templates file.
    |new_schema|: The full contents of the new schema as found  (if any) in the
      modified policy templates file.
    '''

    # If the old schema was present and the new one is no longer present, this
    # is an error. This case can occur while we are recursing through various
    # 'object' type schemas.
    if (new_schema is None):
      self._Error(
          'Policy schema path \'%s\' in old schema was removed in newer '
          'version.' % (current_schema_key))
      return

    # Both old and new schema information must be in dict format.
    if type(old_schema) is not dict:
      self._Error(
          'Policy schema path \'%s\' in old policy is of type \'%s\', it must '
          'be dict type.' % (current_schema_key, type(old_schema)))

    if type(new_schema) is not dict:
      self._Error(
          'Policy schema path \'%s\' in new policy is of type \'%s\', it must '
          'be dict type.' % (current_schema_key, type(new_schema)))

    # Both schemas must either have a 'type' key or not. This covers the case
    # where the scheme is merely a '$ref'
    if ('type' in old_schema) != ('type' in new_schema):
      self._Error(
          'Mismatch in type definition for old schema and new schema for '
          'policy schema path \'%s\'. One schema defines a type while the other'
          ' does not.' % (current_schema_key, old_schema['type'],
                          new_schema['type']))
      return

    # For schemes that define a 'type', make sure they match.
    schema_type = None
    if ('type' in old_schema):
      if (old_schema['type'] != new_schema['type']):
        self._Error(
            'Policy schema path \'%s\' in old schema is of type \'%s\' but '
            'new schema is of type \'%s\'.' %
            (current_schema_key, old_schema['type'], new_schema['type']))
        return
      schema_type = old_schema['type']

    # If a schema does not have 'type' we will simply end up comparing every
    # key/value pair for exact matching (the final else in this loop). This will
    # ensure that '$ref' type schemas match.
    for old_key, old_value in old_schema.items():
      # 'type' key was already checked above.
      if (old_key == 'type'):
        continue

      # If the schema key is marked as modifiable (e.g. 'description'), then
      # no validation is needed. Anything can be done to it include removal.
      if IsKeyDefinedForTypeInDictionary(schema_type, old_key,
                                         MODIFIABLE_SCHEMA_KEYS_PER_TYPE):
        continue

      # If a key was removed in the new schema, check if the removal was
      # allowed. If not this is an error. The removal of some schema keys make
      # the schema less restrictive (e.g. removing 'required' keys in
      # dictionaries or removing 'minimum' in integer schemas).
      if old_key not in new_schema:
        if not IsKeyDefinedForTypeInDictionary(
            schema_type, old_key, REMOVABLE_SCHEMA_VALUES_PER_TYPE):
          self._Error(
              'Key \'%s\' in old policy schema path \'%s\' was removed in '
              'newer version.' % (old_key, current_schema_key))
        continue

      # For a given type that has a key that can define dictionaries of schemas
      # (e.g. 'object' types), we need to validate the schema of each individual
      # property that is defined. We also need to validate that no old
      # properties were removed. Any new properties can be added.
      if IsKeyDefinedForTypeInDictionary(
          schema_type, old_key, KEYS_DEFINING_PROPERTY_DICT_SCHEMAS_PER_TYPE):
        if type(old_value) is not dict:
          self._Error(
              'Unexpected type \'%s\' at policy schema path \'%s\'. It must be '
              'dict' % (type(old_value).__name__,))
          continue

        # Make all old properties exist and are compatible. Everything else that
        # is new requires no validation.
        new_schema_value = new_schema[old_key]
        for sub_key in old_value.keys():
          self._CheckSchemasAreCompatible(
              schema_key_path + [old_key, sub_key], old_value[sub_key],
              new_schema_value[sub_key]
              if sub_key in new_schema_value else None)
      # For types that have a key that themselves define a schema (e.g. 'items'
      # schema in an 'array' type), we need to validate the schema defined in
      # the key.
      elif IsKeyDefinedForTypeInDictionary(schema_type, old_key,
                                           KEYS_DEFINING_SCHEMAS_PER_TYPE):
        self._CheckSchemasAreCompatible(
            schema_key_path + [old_key], old_value,
            new_schema[old_key] if old_key in new_schema else None)
      # For any other key, we just check if the two values of the key are
      # compatible with each other, possibly allowing removal of entries in
      # array values if needed (e.g. removing 'required' fields makes the schema
      # less restrictive).
      else:
        self._CheckSchemaValueIsCompatible(
            schema_key_path + [old_key], old_value, new_schema[old_key],
            IsKeyDefinedForTypeInDictionary(schema_type, old_key,
                                            REMOVABLE_SCHEMA_VALUES_PER_TYPE),
            CUSTOM_VALUE_CHANGE_VALIDATION_PER_TYPE[schema_type][old_key]
            if IsKeyDefinedForTypeInDictionary(
                schema_type, old_key,
                CUSTOM_VALUE_CHANGE_VALIDATION_PER_TYPE) else None)

    for new_key in (old_key for old_key in new_schema.keys()
                    if not old_key in old_schema.keys()):
      self._Error(
          'Key \'%s\' was added to policy schema path \'%s\' in new schema.' %
          (new_key, current_schema_key))

  def _CheckPolicyDefinitionChangeCompatibility(self, original_policy,
                                                original_released_platforms,
                                                new_policy,
                                                new_released_platforms,
                                                current_version):
    '''
    Checks if the new policy definition is compatible with the original policy
    definition.

    Args:
      original_policy: The policy definition as it was in the original policy
                       templates file.
      original_released_platforms: A dictionary contains a released platforms
                                   and their release version in the  original
                                   policy template files.
      new_policy: The policy definition as it is (if any) in the modified policy
                  templates file.
      new_released_platforms: A dictionary contains a released platforms and
                              their release version in the modified policy
                              template files.
      current_version: The current major version of the branch as stored in
      chrome/VERSION.

    '''
    # 1. Check if the supported_on versions are valid.

    # All starting versions in supported_on in the original policy must also
    # appear in the changed policy. The only thing that can be added is an
    # ending version.
    for platform in original_released_platforms:
      if platform not in new_released_platforms:
        self._Error('Released platform %s has been removed.' % (platform),
                    'policy', original_policy['name'])
      elif original_released_platforms[platform] < new_released_platforms[
          platform]:
        self._Error(
            'Supported version of released platform %s is changed to a later '
            'version %d from %d.' % (platform, new_released_platforms[platform],
                                     original_released_platforms[platform]),
            'policy', original_policy['name'])

    #2. Check if the type of the policy has changed.
    if new_policy['type'] != original_policy['type']:
      self._Error(
          'Cannot change the type of released policy \'%s\' from %s to %s.' %
          (new_policy['name'], original_policy['type'], new_policy['type']))

    #3 Check if the policy has suddenly been marked as future: true.
    if ('future' in new_policy
        and new_policy['future']) and ('future' not in original_policy
                                       or not original_policy['future']):
      self._Error('Cannot make released policy \'%s\' a future policy' %
                  (new_policy['name']))

    original_device_only = ('device_only' in original_policy and
                            original_policy['device_only'])

    #4 Check if the policy has changed its device_only value
    if (('device_only' in new_policy and
         original_device_only != new_policy['device_only']) or
        ('device_only' not in new_policy and original_device_only)):
      self._Error(
          'Cannot change the device_only status of released policy \'%s\'' %
          (new_policy['name']))

    #5 Check schema changes for compatibility.
    self._CheckSchemasAreCompatible([original_policy['name']],
                                    original_policy['schema'],
                                    new_policy['schema'])

  def _CheckNewReleasedPlatforms(self, original_platforms, new_platforms,
                                 current_version, policy_name):
    '''If released version has changed, it should be the current version unless
       there is a special reason.'''
    for platform in new_platforms:
      new_version = new_platforms[platform]
      if new_version == original_platforms.get(platform):
        continue
      if new_version == current_version - 1:
        self._Warning(
            'Policy %s on %s will be released in %d which has passed the '
            'branch point. Please merge it into Beta or change the version to '
            '%d.' % (policy_name, platform, new_version, current_version))
      elif new_version < current_version - 1:
        self.non_compatibility_error_count += 1
        self._Error(
            'Version %d has been released to Stable already. Please use '
            'version %d instead for platform %s.' %
            (new_version, current_version, platform), 'policy', policy_name)

  def _CheckDeprecatedFutureField(self, original_policy, new_policy,
                                  policy_name):
    '''The 'future' flag has been deprecated, it shouldn't be used for any new
       policy.'''
    if ('future' in new_policy
        and (original_policy is None or 'future' not in original_policy)):
      self.non_compatibility_error_count += 1
      self._Error(
          "The 'future' flag has been deprecated, please use the 'future_on' "
          "list instead. Search the flag documentation at the top of the "
          "policy_templates.json file for more information.", 'policy',
          policy_name)

  # Checks if the new policy definitions are compatible with the policy
  # definitions coming from the original_file_contents.
  def _CheckPolicyDefinitionsChangeCompatibility(
      self, policy_definitions, original_file_contents, current_version):
    '''
    Checks if all the |policy_definitions| in the modified policy templates file
    are compatible with the policy definitions defined in the original policy
    templates file with |original_file_contents| .

    |policy_definitions|: The policy definition as it is in the modified policy
      templates file.
    |original_file_contents|: The full contents of the original policy templates
      file.
    |current_version|: The current major version of the branch as stored in
      chrome/VERSION.
    '''
    try:
      original_container = eval(original_file_contents)
    except:
      import traceback
      traceback.print_exc(file=sys.stdout)
      self._Error('Invalid Python/JSON syntax in original file.')
      return

    if original_container == None:
      self._Error('Invalid Python/JSON syntax in original file.')
      return

    original_policy_definitions = self._CheckContains(
        original_container,
        'policy_definitions',
        list,
        parent_element=None,
        optional=True,
        container_name='The root element',
        offending=None)

    if original_policy_definitions is None:
      return

    # Sort the new policies by name for faster searches.
    policy_definitions_dict = {
        policy['name']: policy
        for policy in policy_definitions
        if policy['type'] != 'group'
    }

    original_policy_name_set = {
        policy['name']
        for policy in original_policy_definitions if policy['type'] != 'group'
    }

    for original_policy in original_policy_definitions:
      # Check change compatibility for all non-group policy definitions.
      if original_policy['type'] == 'group':
        continue

      original_released_platforms, original_rolling_out_platforms = \
              self._GetReleasedPlatforms( original_policy, current_version)

      new_policy = policy_definitions_dict.get(original_policy['name'])

      # A policy that has at least one released platform cannot be removed.
      if new_policy is None and original_released_platforms:
        self._Error('Released policy \'%s\' has been removed.' %
                    original_policy['name'])
        continue

      new_released_platforms, new_rolling_out_platform = \
              self._GetReleasedPlatforms(new_policy, current_version)

      # Check policy compatibility if there is at least one released platform.
      if original_released_platforms:
        self._CheckPolicyDefinitionChangeCompatibility(
            original_policy, original_released_platforms, new_policy,
            new_released_platforms, current_version)

      # New released platforms should always use the current version unless they
      # are going to be merged into previous milestone.
      if new_released_platforms or new_rolling_out_platform:
        self._CheckNewReleasedPlatforms(
            MergeDict(original_released_platforms,
                      original_rolling_out_platforms),
            MergeDict(new_released_platforms, new_rolling_out_platform),
            current_version, original_policy['name'])

      if new_policy:
        self._CheckDeprecatedFutureField(original_policy, new_policy,
                                         original_policy['name'])

    # Check brand new policies:
    for new_policy_name in set(
        policy_definitions_dict.keys()) - original_policy_name_set:
      new_policy = policy_definitions_dict[new_policy_name]
      new_released_platforms, new_rolling_out_platform = \
              self._GetReleasedPlatforms(new_policy, current_version)
      if new_released_platforms or new_rolling_out_platform:
        self._CheckNewReleasedPlatforms({},
                                        MergeDict(new_released_platforms,
                                                  new_rolling_out_platform),
                                        current_version, new_policy_name)
      self._CheckDeprecatedFutureField(None, new_policy, new_policy_name)

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
    self._Warning('In line %d: Warning: Automatically fixing formatting: %s' %
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
          ("'highest_atomic_group_id_currently_used' must be set to the "
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

  def Main(self, filename, options, original_file_contents, current_version):
    try:
      with open(filename, "rb") as f:
        raw_data = f.read().decode("UTF-8")
        data = eval(raw_data)
        DuplicateKeyVisitor().visit(ast.parse(raw_data))
    except ValueError as e:
      self._Error(str(e))
      return 1
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
        self._CheckPolicy(policy, False, policy_ids, deleted_policy_ids,
                          current_version)
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

    # Third part: if the original file contents are available, try to check
    # if the new policy definitions are compatible with the original policy
    # definitions (if the original file contents have not raised any syntax
    # errors).
    self.non_compatibility_error_count = self.error_count
    if (not self.non_compatibility_error_count
        and original_file_contents is not None and current_version is not None):
      self._CheckPolicyDefinitionsChangeCompatibility(
          policy_definitions, original_file_contents, current_version)

    if self.non_compatibility_error_count != self.error_count:
      print(
          '\nThere were compatibility validation errors in the change. You may '
          'bypass this validation by adding "BYPASS_POLICY_COMPATIBILITY_CHECK='
          '<justification>" to your changelist description. If you believe '
          'that this validation is a bug, please file a crbug against '
          '"Enterprise>CloudPolicy" and add a link to the bug as '
          'justification. Otherwise, please provide an explanation for the '
          'change. For more information please refer to: '
          'https://bit.ly/33qr3ZV.')

    # Fourth part: summary and exit.
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

  def Run(self,
          argv,
          filename=None,
          original_file_contents=None,
          current_version=None):
    parser = argparse.ArgumentParser(
        usage='usage: %prog [options] filename',
        description='Syntax check a policy_templates.json file.')
    parser.add_argument(
        '--device_policy_proto_path',
        help='[REQUIRED] File path of the device policy proto file.')
    parser.add_argument(
        '--fix', action='store_true', help='Automatically fix formatting.')
    parser.add_argument(
        '--backup',
        action='store_true',
        help='Create backup of original file (before fixing).')
    parser.add_argument(
        '--stats', action='store_true', help='Generate statistics.')
    args = parser.parse_args(argv)
    if filename is None:
      print('Error: Filename not specified.')
      return 1
    if args.device_policy_proto_path is None:
      print('Error: Missing --device_policy_proto_path argument.')
      return 1
    return self.Main(filename, args, original_file_contents, current_version)

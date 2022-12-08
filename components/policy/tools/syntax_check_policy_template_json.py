#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
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

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party'))
import pyyaml

LEADING_WHITESPACE = re.compile('^([ \t]*)')
TRAILING_WHITESPACE = re.compile('.*?([ \t]+)$')
# Matches all non-empty strings that contain no whitespaces.
NO_WHITESPACE = re.compile('[^\s]+$')

SOURCE_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(__file__))))

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
    'SendMouseEventsDisabledFormControlsEnabled',
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

# List of 'integer' policies that allow a negative 'minimum' value.
LEGACY_NEGATIVE_MINIMUM_ALLOWED = [
    'PrintJobHistoryExpirationPeriod',
    'GaiaOfflineSigninTimeLimitDays',
    'SAMLOfflineSigninTimeLimit',
    'GaiaLockScreenOfflineSigninTimeLimitDays',
    'SamlLockScreenOfflineSigninTimeLimitDays',
]

# Legacy boolean policies that don't describe the enable/disable case
# specifically.
LEGACY_NO_ENABLE_DISABLE_DESC = [
    'DisablePluginFinder', 'IntegratedWebAuthenticationAllowed'
]

# Device policies which are not prefixed 'Device'.
LEGACY_DEVICE_POLICY_NAME_OFFENDERS = [
    'ChromadToCloudMigrationEnabled',
    'AutoCleanUpStrategy',
    'EnableDeviceGranularReporting',
    'ReportCRDSessions',
    'ReportUploadFrequency',
    'HeartbeatEnabled',
    'HeartbeatFrequency',
    'LogUploadEnabled',
    'ChromeOsReleaseChannel',
    'ChromeOsReleaseChannelDelegated',
    'KioskCRXManifestUpdateURLIgnored',
    'ManagedGuestSessionPrivacyWarningsEnabled',
    'SystemTimezone',
    'SystemUse24HourClock',
    'UptimeLimit',
    'RebootAfterUpdate',
    'AttestationEnabledForDevice',
    'AttestationForContentProtectionEnabled',
    'SupervisedUsersEnabled',
    'ExtensionCacheSize',
    'DisplayRotationDefault',
    'AllowKioskAppControlChromeVersion',
    'LoginAuthenticationBehavior',
    'UsbDetachableWhitelist',
    'UsbDetachableAllowlist',
    'SystemTimezoneAutomaticDetection',
    'NetworkThrottlingEnabled',
    'LoginVideoCaptureAllowedUrls',
    'TPMFirmwareUpdateSettings',
    'MinimumRequiredChromeVersion',
    'CastReceiverName',
    'UnaffiliatedArcAllowed',
    'VirtualMachinesAllowed',
    'PluginVmAllowed',
    'PluginVmLicenseKey',
    'SystemProxySettings',
    'RequiredClientCertificateForDevice',
    'ReportDeviceVersionInfo',
    'ReportDeviceActivityTimes',
    'ReportDeviceAudioStatus',
    'ReportDeviceAudioStatusCheckingRateMs',
    'ReportDeviceBootMode',
    'ReportDeviceLocation',
    'ReportDeviceNetworkConfiguration',
    'ReportDeviceNetworkInterfaces',
    'ReportDeviceNetworkStatus',
    'ReportDeviceNetworkTelemetryCollectionRateMs',
    'ReportDeviceNetworkTelemetryEventCheckingRateMs',
    'ReportDeviceUsers',
    'ReportDeviceHardwareStatus',
    'ReportDeviceSessionStatus',
    'ReportDeviceSecurityStatus',
    'ReportDeviceGraphicsStatus',
    'ReportDeviceCrashReportInfo',
    'ReportDeviceOsUpdateStatus',
    'ReportDevicePowerStatus',
    'ReportDevicePeripherals',
    'ReportDeviceStorageStatus',
    'ReportDeviceBoardStatus',
    'ReportDeviceCpuInfo',
    'ReportDeviceTimezoneInfo',
    'ReportDeviceMemoryInfo',
    'ReportDeviceBacklightInfo',
    'ReportDeviceAppInfo',
    'ReportDeviceBluetoothInfo',
    'ReportDeviceFanInfo',
    'ReportDeviceVpdInfo',
    'ReportDeviceSystemInfo',
    'ReportDevicePrintJobs',
    'ReportDeviceLoginLogout',
    'ReportDeviceSignalStrengthEventDrivenTelemetry',
]

# User policies which are prefixed with 'Device'.
LEGACY_USER_POLICY_NAME_OFFENDERS = [
    'DeviceLocalAccountManagedSessionEnabled',
    'DeviceAttributesAllowedForOrigins',
    'DevicePowerAdaptiveChargingEnabled',
]

# List of policies where not all properties are required to be presented in the
# example value. This could be useful e.g. in case of mutually exclusive fields.
# See crbug.com/1068257 for the details.
OPTIONAL_PROPERTIES_POLICIES_ALLOWLIST = ['ProxySettings']

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
# to a policy schema without causing incompatibilities).
MODIFIABLE_SCHEMA_KEYS_PER_TYPE = {
    'integer': ['description', 'sensitiveValue'],
    'string': ['description', 'sensitiveValue'],
    'object': ['description', 'sensitiveValue'],
    'boolean': ['description']
}

# Defines keys per type that themselves define a further dictionary of
# properties each with their own schemas. For example, 'object' types define
# a 'properties' key that list all the possible keys in the object.
KEYS_DEFINING_PROPERTY_DICT_SCHEMAS_PER_TYPE = {
    'object': ['properties', 'patternProperties']
}

# Defines keys per type that themselves define a schema. For example, 'array'
# types define an 'items' key defines the schema for each item in the array.
KEYS_DEFINING_SCHEMAS_PER_TYPE = {
    'object': ['additionalProperties'],
    'array': ['items']
}

# The list of platforms policy could support.
ALL_SUPPORTED_PLATFORMS = [
    'chrome_frame', 'chrome_os', 'android', 'webview_android', 'ios', 'fuchsia',
    'chrome.win', 'chrome.win7', 'chrome.linux', 'chrome.mac', 'chrome.*'
]

# The list of platforms that chrome.* represents.
CHROME_STAR_PLATFORMS = ['chrome.win', 'chrome.mac', 'chrome.linux']

# List of supported metapolicy types.
METAPOLICY_TYPES = ['merge', 'precedence']

# List of supported chrome os management tags.
SUPPORTED_CHROME_OS_MANAGEMENT = ['google_cloud', 'active_directory']

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


def _GetPolicyValueType(policy_type):
  if policy_type == 'main':
    return bool
  elif policy_type in ('string', 'string-enum'):
    return str
  elif policy_type in ('int', 'int-enum'):
    return int
  elif policy_type in ('list', 'string-enum-list'):
    return list
  elif policy_type == 'external':
    return dict
  elif policy_type == 'dict':
    return [dict, list]
  else:
    raise NotImplementedError('Unknown value type for policy type: %s' %
                              policy_type)


def _GetPolicyItemType(policy_type):
  if policy_type == 'main':
    return bool
  elif policy_type in ('string-enum', 'string-enum-list'):
    return str
  elif policy_type in ('int-enum'):
    return int
  else:
    raise NotImplementedError('Unknown item type for policy type: %s' %
                              policy_type)


def MergeDict(*dicts):
  result = {}
  for dictionary in dicts:
    result.update(dictionary)
  return result


def LenWithoutPlaceholderTags(text):
  PATTERN = re.compile('<ph [^>]*>')
  length = len(text)

  for match in PATTERN.finditer(text):
    length -= len(match.group(0))

  length -= 5 * text.count('</ph>')

  return length


def _IsAllowedDevicePolicyPrefix(name):
  return name.startswith('Device')


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


class PolicyTypeProvider():
  def __init__(self):
    # TODO(crbug.com/1171839): Persist the deduced schema types into a separate
    # file to further speed up the presubmit scripts.
    self._policy_types = {}
    # List of policies which are type 'dict' but should be type 'external'
    # according to their schema. There are several reasons for such exceptions:
    # - The file being downloaded is large (on the order of GB)
    # - The downloaded file shouldn't be publicly accessible
    self._external_type_mismatch_allowlist = ['PluginVmImage']

  def GetPolicyType(self, policy, schemas_by_id={}):
    '''Gets the type of `policy` according to its schema.

    Args:
      policy (dict): The policy to get the type for.
      schemas_by_id (dict): Maps schema id to a a schema.

    '''
    # Policies may have the same name as the groups they belong to, so caching
    # would not work. Instead, first check if the policy is a group; if it's
    # not, go ahead with caching.
    if self._IsGroup(policy):
      return 'group'

    policy_name = policy.get('name')
    if not policy_name or policy_name not in self._policy_types:
      return self._policy_types.setdefault(
          policy_name, self._GetPolicyTypeFromSchema(policy, schemas_by_id))
    return self._policy_types[policy_name]

  def _IsGroup(self, policy):
    return policy.get('type') == 'group'

  def _GetPolicyTypeFromSchema(self, policy, schemas_by_id):
    schema = policy.get('schema')
    if not schema:
      raise NotImplementedError(
          'Policy %s does not have a schema. A schema must be implemented for '
          'all non-group type policies.' % policy.get('name'))

    if '$ref' in schema:
      if not schema['$ref'] in schemas_by_id:
        raise NotImplementedError(
            'Policy %s uses unknow $ref %s in schema' % policy['name'],
            schema['$ref'])
      schema = schemas_by_id[schema['$ref']]

    schema_type = schema.get('type')
    if schema_type == 'boolean':
      return 'main'
    elif schema_type == 'integer':
      items = policy.get('items')
      if items and all([
          item.get('name') and item.get('value') is not None for item in items
      ]):
        return 'int-enum'
      return 'int'
    elif schema_type == 'string':
      items = policy.get('items')
      if items and all([
          item.get('name') and item.get('value') is not None for item in items
      ]):
        return 'string-enum'
      return 'string'
    elif schema_type == 'array':
      schema_items = schema.get('items')
      if schema_items.get('type') == 'string' and schema_items.get('enum'):
        return 'string-enum-list'
      elif schema_items.get('type') == 'object' and schema_items.get(
          'properties'):
        return 'dict'
      elif ('$ref' in schema_items
            and schemas_by_id[schema_items['$ref']].get('type') == 'object'):
        return 'dict'
      return 'list'
    elif schema_type == 'object':
      schema_properties = schema.get('properties')
      if schema_properties and schema_properties.get(
          'url') and schema_properties.get('hash') and policy.get(
              'name') not in self._external_type_mismatch_allowlist:
        return 'external'
      return 'dict'


class PolicyTemplateChecker(object):

  def __init__(self):
    self.num_policies = 0
    self.num_groups = 0
    self.num_policies_in_groups = 0
    self.options = None
    self.features = []
    self.schema_validator = SchemaValidator()
    self.has_schema_error = False
    self.policy_type_provider = PolicyTypeProvider()
    self.errors = []
    self.warnings = []

  def _Warning(self, message):
    self.warnings.append(f'Warning: {message}')

  def _Error(self,
             message,
             parent_element=None,
             identifier=None,
             offending_snippet=None):
    error_prompt = ''
    if identifier is not None and parent_element is not None:
      error_prompt += f'In {parent_element} {identifier}: '

    formatted_error_message = f'Error: {error_prompt}{message}'
    if offending_snippet is not None:
      if isinstance(offending_snippet, dict) or isinstance(
          offending_snippet, list):
        yaml_str = pyyaml.dump(offending_snippet, indent=2)
        formatted_error_message += f'\n  Offending: {yaml_str}'
      else:
        formatted_error_message += f'\n  {offending_snippet}'
    self.errors.append(formatted_error_message)

  def _LineError(self, message, line_number):
    self._Error(f'In line {line_number}: {message}')

  def _LineWarning(self, message, line_number):
    self._Warning(f'In line {line_number}: Automatically fixing formatting: '
                  f'{message}')

  def _PolicyError(self, message, policy, field=None, value=None):
    '''
    Log an error `message for `policy`.

    Set `field` if the error is found for a certain policy `field`.
    Set `value` if the error is found for a certain policy `field` with `value`.
    '''
    field_str = None
    if field:
      if value is None:
        value = policy.get(field, "<not set>")
      field_str = json.dumps({field: value})[1:-1]
    self._Error(message, 'policy', policy.get('name', '<No name>'), field_str)

  def _SchemaCompatibleError(self, message):
    self.schema_compatible_errors.append(message)

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

    Returns: |container[key]| if the key is present and there are no errors,
             None otherwise.
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
            '%s does not have a %s "%s".' %
            (container_name.title(), value_type.__name__, key), container_name,
            identifier, offending)
      return None
    value = container[key]
    value_types = value_type if isinstance(value_type, list) else [value_type]
    if not any(isinstance(value, type) for type in value_types):
      self._Error(
          'Value of "%s" is not one of [ %s ].' %
          (key, ', '.join([type.__name__ for type in value_types])),
          container_name, identifier, value)
      return None
    if str in value_types and regexp_check and not regexp_check.match(value):
      self._Error(
          'Value of "%s" does not match "%s".' % (key, regexp_check.pattern),
          container_name, identifier, value)
      return None
    return value


  def _ValidateSchema(self, schema, schema_name, policy, schemas_by_id):
    ''' Helper fuction to call `schema_validator.ValidateSchema`. Appends error
        to `self.errors` if necessary.
    '''
    schema_errors = self.schema_validator.ValidateSchema(schema, schemas_by_id)
    if schema_errors:
      schema_error_message = "\n  ".join(schema_errors)
      self._PolicyError(
          f'{schema_name.capitalize()} is invalid\n'
          f'  {schema_error_message}', policy)
      self.has_schema_error = True

  def _ValidateValue(self, schema, example, enforce_use_entire_schema,
                     schema_name, policy):
    '''Helper function to call `schema_validator.ValidateValue()` Appends error
       to `self.errors` if needed.
    '''
    value_errors = self.schema_validator.ValidateValue(
        schema, example, enforce_use_entire_schema)
    if value_errors:
      value_error_message = "\n  ".join(value_errors)
      self._PolicyError(
          f'Example does not comply to the policy\'s {schema_name} or '
          'does not use all properties at least once.\n'
          f'  {value_error_message}', policy)

  def _CheckPolicySchema(self, policy, policy_type, schemas_by_id):
    '''Checks that the 'schema' field matches the 'type' field.'''
    self.has_schema_error = False

    if policy_type == 'group':
      self._Error('Schema should not be defined for group type policy %s.' %
                  policy.get('name'))
      self.has_schema_error = True
      return

    schema = self._CheckContains(policy, 'schema', dict)
    if not schema:
      # Schema must be defined for all non-group type policies. An appropriate
      # |_Error| message is populated in the |_CheckContains| call above, so it
      # is not repeated here.
      self.has_schema_error = True
      return

    policy_type_legacy = policy.get('type')
    # TODO(crbug.com/1310258): Remove this check once 'type' is removed from
    # policy_templates.
    if policy_type != policy_type_legacy:
      self._PolicyError(
          f'Unexpected type. Type "{policy_type}" was expected based on the '
          'schema.', policy, 'type')

    self._ValidateSchema(schema, 'schema', policy, schemas_by_id)

    if 'validation_schema' in policy:
      self._ValidateSchema(policy.get('validation_schema'), 'validation schema',
                           policy, schemas_by_id)

    # Checks that boolean policies are not negated (which makes them harder to
    # reason about).
    if (policy_type == 'main' and 'disable' in policy.get('name').lower()
        and policy.get('name') not in LEGACY_INVERTED_POLARITY_ALLOWLIST):
      self._PolicyError(
          'Boolean policy uses negative polarity name, please follow the '
          'XYZEnabled pattern. See http://crbug.com/85687', policy, 'name')

    # Checks that the policy doesn't have a validation_schema - the whole
    # schema should be defined in 'schema'- unless listed as legacy.
    if ('validation_schema' in policy
        and policy.get('name') not in LEGACY_EMBEDDED_JSON_ALLOWLIST):
      self._PolicyError(
          '"validation_schema" is no longer recommended, use '
          '"schema" instead.', policy)

    # Try to make sure that any policy with a complex schema is storing it as
    # a 'dict', not embedding it inside JSON strings - unless listed as legacy.
    if (self._AppearsToContainEmbeddedJson(policy.get('example_value'))
        and policy.get('name') not in LEGACY_EMBEDDED_JSON_ALLOWLIST):
      self._PolicyError(
          'Example value is JSON string.\n'
          '  Do not store complex data as '
          'stringified JSON - instead, store it in a dict and '
          'define it in "schema".', policy, 'schema')

    # Checks that integer policies do not allow negative values.
    if (policy_type == 'int' and schema.get('minimum', 0) < 0
        and policy.get('name') not in LEGACY_NEGATIVE_MINIMUM_ALLOWED):
      self._PolicyError(
          f'Integer policy allows negative values.\n'
          '  Negative values are forbidden and could silently be replaced with '
          'zeros when using them. See also https://crbug.com/1115976', policy,
          'schema')

  def _CheckTotalDevicePolicyExternalDataMaxSize(self, policy_definitions):
    total_device_policy_external_data_max_size = 0
    for policy in policy_definitions:
      if (policy.get('device_only', False)
          and self.policy_type_provider.GetPolicyType(policy) == 'external'):
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
          self._AppearsToContainEmbeddedJson(v) for v in example_value.values())

  # Checks that there are no duplicate proto paths in device_policy_proto_map.
  def _CheckDevicePolicyProtoMappingUniqueness(self, device_policy_proto_map,
                                               legacy_device_policy_proto_map):
    # Check that device_policy_proto_map does not have duplicate values.
    proto_paths = set()
    for proto_path in device_policy_proto_map.values():
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
  # field in device_policy_proto_map.yaml.
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
    with open(device_policy_proto_path, 'r', encoding='utf-8') as file:
      device_policy_proto = file.read()

    for policy, proto_path in device_policy_proto_map.items():
      fields = proto_path.split(".")
      for field in fields:
        if field not in device_policy_proto:
          self._Error("Bad device_policy_proto_map for policy '%s': "
                      "Field '%s' not present in device policy proto." %
                      (policy, field))

  def _NeedsDefault(self, policy):
    return self.policy_type_provider.GetPolicyType(policy) in ('int', 'main',
                                                               'string-enum',
                                                               'int-enum')

  def _CheckDefault(self, policy, current_version):
    if not self._NeedsDefault(policy):
      return

    # If a policy should have a default but it is no longer supported, we can
    # safely ignore this error.
    if ('default' not in policy
        and not self._SupportedPolicy(policy, current_version)):
      return

    # Only validate the default when present.
    # TODO(crbug.com/1139046): Always validate the default for types that
    # should have it.
    if 'default' not in policy:
      return
    policy_type = self.policy_type_provider.GetPolicyType(policy)
    default = policy.get('default')
    if policy_type == 'int':
      # A default value of None is acceptable when the default case is
      # equivalent to the policy being unset and there is no numeric equivalent.
      if default is None:
        return

      if not isinstance(default, int):
        self._PolicyError('Default value it not an integer.', policy, 'default')
      elif default < 0:
        self._PolicyError(f'Default value less than zero.', policy, 'default')
      return

    if policy_type == 'main':
      # If the policy doesn't have items but is no longer supported, predefined
      # values are used. Otherwise the policy must have items defined.
      if 'items' not in policy and not self._SupportedPolicy(
          policy, current_version):
        acceptable_values = (True, False, None)
      else:
        acceptable_values = [x['value'] for x in policy['items']]
    elif policy_type in ('string-enum', 'int-enum'):
      acceptable_values = [None] + [x['value'] for x in policy['items']]
    else:
      raise NotImplementedError('Unimplemented policy type: %s' % policy_type)

    if default not in acceptable_values:
      self._PolicyError(f'Default value is not one of {acceptable_values}',
                        policy, 'default')

  def _NeedsItems(self, policy):
    return (not policy.get('deprecated', False)
            and self.policy_type_provider.GetPolicyType(policy) in (
                'main', 'int-enum', 'string-enum', 'string-enum-list'))

  def _CheckItems(self, policy, current_version):
    if not self._NeedsItems(policy):
      return

    # If a policy should have items, but it is no longer supported, we
    # can safely ignore this error.
    if 'items' not in policy and not self._SupportedPolicy(
        policy, current_version):
      return

    items = self._CheckContains(policy, 'items', list)
    if items is None:
      return

    if len(items) < 1:
      self._PolicyError('"items" is empty.', policy, 'items')
      return

    # Ensure all items have valid captions.
    for item in items:
      self._CheckContains(item,
                          'caption',
                          str,
                          container_name='item',
                          identifier=policy.get('name'))

    policy_type = self.policy_type_provider.GetPolicyType(policy)
    if policy_type == 'main':
      # Main (bool) policies must contain a list of items to clearly
      # indicate what the states mean.
      required_values = [True, False]

      # The unset item can only appear if the default is None, since
      # there is no other way for it to be set.
      if 'default' in policy and policy['default'] == None:
        required_values.append(None)

      # Since the item captions don't appear everywhere the description does,
      # try and ensure the items are still described in the descriptions.
      value_to_names = {
          None: {'none', 'unset', 'not set', 'not configured'},
          True: {'true', 'enable'},
          False: {'false', 'disable'},
      }
      if policy['name'] not in LEGACY_NO_ENABLE_DISABLE_DESC:
        for value in required_values:
          names = value_to_names[value]
          if not any(name in policy['desc'].lower() for name in names):
            self._PolicyError(
                'Description does not describe what happens when it is '
                f'set to {value}. If possible update the description to '
                f'describe this while using at least one of {names}', policy,
                'desc')

      values_seen = set()
      for item in items:
        # Bool items shouldn't have names, since it's the same information
        # as the value field.
        if 'name' in item:
          self._PolicyError('Item has an unnecessary "name" field.', policy,
                            'items', [item])

        # Each item must have a value.
        if 'value' not in item:
          self._PolicyError('Item does not have "value" field', policy, 'items',
                            [item])
        else:
          value = item['value']
          if value in values_seen:
            self._PolicyError(f'Duplicate item value {value}', policy, 'items',
                              [item])
          else:
            values_seen.add(value)
            if value not in required_values:
              self._PolicyError(
                  f'Unexpected item value {value}. must be one of '
                  f'{required_values}', policy, 'items', [item])

      if not values_seen.issuperset(required_values):
        self._PolicyError('Missing item values {required_values - values_seen}',
                          policy, 'items')

    if policy_type in ('int-enum', 'string-enum', 'string-enum-list'):
      for item in items:
        # Each item must have a name.
        self._CheckContains(item,
                            'name',
                            str,
                            container_name='item',
                            identifier=policy.get('name'),
                            regexp_check=NO_WHITESPACE)

        # Each item must have a value of the correct type.
        self._CheckContains(item,
                            'value',
                            _GetPolicyItemType(policy_type),
                            container_name='item',
                            identifier=policy.get('name'))

  def _CheckOwners(self, policy):
    owners = self._CheckContains(policy, 'owners', list)
    if not owners:
      return

    for owner in owners:
      FILE_PREFIX = 'file://'
      if owner.startswith(FILE_PREFIX):
        file_path = owner[len(FILE_PREFIX):]
        full_file_path = os.path.join(SOURCE_DIR, file_path)
        if not (os.path.exists(full_file_path)):
          self._Warning(
              'Policy %s lists non-existant owners files, %s, as an owner. '
              'Please either add the owners file or remove it from this list.' %
              (policy.get('name'), full_file_path))
      elif '@' in owner:
        # TODO(pastarmovj): Validate the email is a committer's.
        pass
      else:
        self._PolicyError(
            'Unexpected owner, %s, all owners should '
            'be committer emails or OWNERS path with file://', policy, 'owners')

  def _SupportedPolicy(self, policy, current_version):
    # If a policy has any future_on platforms, it is still supported.
    if len(policy.get('future_on', [])) > 0:
      return True

    for s in policy.get('supported_on', []):
      _, _, supported_on_to = _GetSupportedVersionPlatformAndRange(s)

      # If supported_on_to isn't given, this policy is still supported.
      if supported_on_to is None:
        return True

      # If supported_on_to is equal or greater than the current version, it's
      # still supported.
      if current_version <= int(supported_on_to):
        return True

    return False

  def _CheckPolicyDefinition(self,
                             policy,
                             current_version,
                             schemas_by_id,
                             is_in_group=False):
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
          'future_on',
          'id',
          'schema',
          'validation_schema',
          'description_schema',
          'url_schema',
          'max_size',
          'tags',
          'default',
          'default_for_enterprise_users',
          'default_for_managed_devices_doc_only',
          'default_policy_level',
          'arc_support',
          'supported_chrome_os_management',
      ):
        self._PolicyError(f'Unknown key: {key}', policy, key)

    # Each policy must have a name.
    self._CheckContains(policy, 'name', str, regexp_check=NO_WHITESPACE)

    # Each policy must have a type.
    policy_types = ('group', 'main', 'string', 'int', 'list', 'int-enum',
                    'string-enum', 'string-enum-list', 'dict', 'external')
    policy_type = self.policy_type_provider.GetPolicyType(policy, schemas_by_id)
    if policy_type not in policy_types:
      self._PolicyError('Policy type is not one of: ' + ', '.join(policy_types),
                        policy)
      return  # Can't continue for unsupported type.

    # Each policy must have a caption message.
    self._CheckContains(policy, 'caption', str)

    # Each policy's description should be within the limit.
    desc = self._CheckContains(policy, 'desc', str)
    if LenWithoutPlaceholderTags(desc) > POLICY_DESCRIPTION_LENGTH_SOFT_LIMIT:
      self._PolicyError(
          'Length of description is more than '
          f'{POLICY_DESCRIPTION_LENGTH_SOFT_LIMIT} characters. Please create a '
          'help center article instead.', policy, {'desc': desc[:50] + '...'})

    # If 'label' is present, it must be a string.
    self._CheckContains(policy, 'label', str, True)

    # If 'deprecated' is present, it must be a bool.
    self._CheckContains(policy, 'deprecated', bool, True)

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

    # policy_type != group
    else:
      # Each policy must have an owner.
      self._CheckOwners(policy)

      # Each policy must have a tag list.
      self._CheckContains(policy, 'tags', list)

      # 'schema' is the new 'type'.
      # TODO(crbug.com/1310258): remove 'type' from policy_templates and
      # all supporting files (including this one), and exclusively use 'schema'.
      self._CheckPolicySchema(policy, policy_type, schemas_by_id)

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
            self._PolicyError('One entry in "supported_on" has no platform',
                              policy, 'supported_on', [supported_on])
          elif not isinstance(supported_on_from, int):
            self._PolicyError(
                'Entries in "supported_on" have an invalid starting version',
                policy, 'supported_on', [supported_on])
          elif isinstance(supported_on_to,
                          int) and supported_on_to < supported_on_from:
            self._PolicyError(
                'Entries in "supported_on" have an invalid ending version',
                policy, 'supported_on', [supported_on])

        if (not self._SupportedPolicy(policy, current_version)
            and not policy.get('deprecated', False)):
          self._PolicyError(
              "Marked as no longer supported, but isn't marked as "
              '"deprecated.\n'
              '  Unsupported policies must be marked as "deprecated": True. '
              'You may see this error after branch point. Please fix the'
              f' issue and cc the policy owners.', policy, 'supported_on')

      supported_platforms = ExpandChromeStar(supported_platforms)
      future_on = ExpandChromeStar(
          self._CheckContains(policy, 'future_on', list, optional=True))

      self._CheckPlatform(supported_platforms, 'supported_on', policy)
      self._CheckPlatform(future_on, 'future_on', policy)

      if not supported_platforms and not future_on:
        self._PolicyError(
            'No valid platform in "supported_on" or '
            '"future_on"', policy)

      if supported_on == []:
        self._Warning("Policy %s: supported_on' is empty." %
                      (policy.get('name')))

      if future_on == []:
        self._Warning("Policy %s: 'future_on' is empty." % (policy.get('name')))

      if future_on:
        for platform in set(supported_platforms).intersection(future_on):
          self._PolicyError(
              f'Platform {platform} is marked as "supported_on" and '
              '"future_on". Put released platform in "supported_on" only',
              policy, 'future_on')


      # Each policy must have a 'features' dict.
      features = self._CheckContains(policy, 'features', dict)

      # All the features must have a documenting message.
      if features:
        for feature in features:
          if not feature in self.features:
            self._PolicyError(
                f'Unknown feature. Known features must have a '
                'documentation string in the messages dictionary.', policy,
                'features', {feature: features[feature]})

      can_be_recommended = self._CheckContains(features,
                                               'can_be_recommended',
                                               bool,
                                               optional=True,
                                               container_name='features')
      can_be_mandatory = self._CheckContains(features,
                                             'can_be_mandatory',
                                             bool,
                                             optional=True,
                                             container_name='features')

      can_be_recommended = False if (
          can_be_recommended) is None else can_be_recommended
      can_be_mandatory = True if can_be_mandatory is None else can_be_mandatory

      if not can_be_recommended and not can_be_mandatory:
        self._PolicyError('Policy can not be mandatory or recommended.', policy,
                          'features')


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
        self._PolicyError(
            '"per_profile" attribute is set with device_only=True', policy,
            'features')

      # If 'device only' policy is on, 'default_for_enterprise_users' shouldn't
      # exist.
      if (policy.get('device_only', False) and
          'default_for_enterprise_users' in policy):
        self._PolicyError(
            'default_for_enteprise_users is set with device_only=True.\n'
            '  Please use default_for_managed_devices_doc_only to document a'
            'differing default value for enrolled devices. Please note '
            'that default_for_managed_devices_doc_only is for '
            'documentation only - it has no side effects, so you will '
            ' still have to implement the enrollment-dependent default '
            'value handling yourself in all places where the device '
            'policy proto is evaluated. This will probably include '
            'device_policy_decoder.cc for chrome, but could '
            'also have to done in other components if they read the '
            'proto directly. Details: crbug.com/809653', policy,
            'default_for_enterprise_users')

      default_policy_level = self._CheckContains(
          policy,
          'default_policy_level',
          str,
          optional=True,
          regexp_check=re.compile('^(recommended|mandatory)$'))

      if default_policy_level:
        if 'default_for_enterprise_users' not in policy:
          self._PolicyError(
              '"default_policy_level" is set without '
              'default_for_enterprise_users.', policy, 'default_policy_level')
        if (default_policy_level == 'recommended' and not can_be_recommended):
          self._PolicyError(
              '"default_policy_level" is set to "recommended" while policy is '
              'not recommendable', policy, 'default_policy_level')
        if (default_policy_level == 'mandatory' and not can_be_mandatory):
          self._PolicyError(
              '"default_policy_level" is set to "mandatory" while policy is '
              'not mandatoryable', policy, 'default_policy_level')
      else:
        if 'default_for_enterprise_users' in policy and not can_be_mandatory:
          self._PolicyError(
              '"default_policy_level" is missing while policy is not '
              'mandatoryable.', policy, 'default_for_enterprise_users')

      if (not policy.get('device_only', False) and
          'default_for_managed_devices_doc_only' in policy):
        self._PolicyError(
            '"default_for_managed_devices_doc_only" is set for non-device '
            'policy', policy, 'default_for_managed_devices_doc_only')

      if (policy.get('device_only', False)
          and not _IsAllowedDevicePolicyPrefix(policy.get('name'))
          and policy.get('name') not in LEGACY_DEVICE_POLICY_NAME_OFFENDERS):
        self._PolicyError('Device policy name is not prefixed with "Device"',
                          policy, 'name')

      if (_IsAllowedDevicePolicyPrefix(policy.get('name'))
          and not policy.get('device_only', False)
          and policy.get('name') not in LEGACY_USER_POLICY_NAME_OFFENDERS):
        self._PolicyError('Non-device policy name is prefixed with "Device"',
                          policy, 'name')

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
      internal_only = self._CheckContains(features,
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

      # 'metapolicy_type' feature must be one of the supported types.
      metapolicy_type = self._CheckContains(features,
                                            'metapolicy_type',
                                            str,
                                            optional=True,
                                            container_name='features')
      if metapolicy_type and metapolicy_type not in METAPOLICY_TYPES:
        self._PolicyError(
            '"metapolicy_type" is not supported. '
            f'Please use one of {METAPOLICY_TYPES}', policy, 'features')

      if cloud_only and platform_only:
        self._PolicyError(
            '"cloud_only" and "platfrom_only" are true at the same time.',
            policy, 'features')

      if is_unlisted and not cloud_only:
        self._PolicyError('"unlisted" is used by non cloud only policy.',
                          policy, 'features')


      # Chrome OS policies may have a non-empty supported_chrome_os_management
      # list with either 'active_directory' or 'google_cloud' or both.
      supported_chrome_os_management = self._CheckContains(
          policy, 'supported_chrome_os_management', list, True)
      if supported_chrome_os_management is not None:
        # Must be on Chrome OS.
        if (supported_on is not None
            and not any('chrome_os' == str
                        for str in (supported_platforms +
                                    (future_on if future_on else [])))):
          self._PolicyError(
              '"supported_chrome_os_management" is used for policy that does '
              'not support Chrome OS.', policy, 'supported_on')
        # Must be non-empty.
        if len(supported_chrome_os_management) == 0:
          self._PolicyError('"supported_chrome_os_management" is empty', policy,
                            'supported_chrome_os_management')
        # Must be either 'active_directory' or 'google_cloud'.
        if (any(str not in SUPPORTED_CHROME_OS_MANAGEMENT
                for str in supported_chrome_os_management)):
          self._PolicyError(
              '"supported_chrome_os_management" contains supported entry.\n'
              f'Please use one of {SUPPORTED_CHROME_OS_MANAGEMENT}', policy,
              'supported_chrome_os_management')

      # Each policy must have an 'example_value' of appropriate type.
      self._CheckContains(policy, 'example_value',
                          _GetPolicyValueType(policy_type))

      # Verify that the example complies with the schema and that all properties
      # are used at least once, so the examples are as useful as possible for
      # admins.
      schema = policy.get('schema')
      example = policy.get('example_value')
      enforce_use_entire_schema = policy.get(
          'name') not in OPTIONAL_PROPERTIES_POLICIES_ALLOWLIST

      if not self.has_schema_error:
        self._ValidateValue(schema, example, enforce_use_entire_schema,
                            'schema', policy)

        if 'validation_schema' in policy and 'description_schema' in policy:
          self._PolicyError(
              '"validation_schema" and "description_schema" both defined.',
              policy)
        secondary_schema = policy.get('validation_schema',
                                      policy.get('description_schema'))
        if secondary_schema:
          real_example = {}
          if policy_type == 'string':
            real_example = json.loads(example)
          elif policy_type == 'list':
            real_example = [json.loads(entry) for entry in example]
          else:
            self._PolicyError(
                'Unsupported type for legacy embedded json policy.', policy)
          self._ValidateValue(secondary_schema, real_example, True,
                              'validation_schema', policy)

      self._CheckDefault(policy, current_version)

      # Statistics.
      self.num_policies += 1
      if is_in_group:
        self.num_policies_in_groups += 1

      self._CheckItems(policy, current_version)

      if policy_type == 'external':
        # Each policy referencing external data must specify a maximum data
        # size.
        self._CheckContains(policy, 'max_size', int)
      elif 'max_size' in policy:
        self._PolicyError('"max_size" is used for non external policies.',
                          policy, 'max_size')

  def _CheckPolicy(self, policy, current_version):
    self._CheckPolicyDefinition(policy, current_version, {})

  def _CheckPlatform(self, platforms, field_name, policy):
    ''' Verifies the |platforms| list. Records any error with |field_name| and
        |policy_name|.  '''
    if not platforms:
      return

    duplicated = set()
    for platform in platforms:
      if len(platform) == 0:
        continue
      if platform not in ALL_SUPPORTED_PLATFORMS:
        self._PolicyError(
            f'Platform "{platform}" is not supported in {field_name}. Valid '
            f'platforms are {ALL_SUPPORTED_PLATFORMS}.', policy, field_name)
      if platform in duplicated:
        self._PolicyError(
            f'Platform "{platform}" appears more than once in {field_name}.',
            policy, field_name)
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
    if policy == None:
      return released_platforms, rolling_out_platform

    for supported_on in policy.get('supported_on', []):
      (supported_platform, supported_from,
       _) = _GetSupportedVersionPlatformAndRange(supported_on)
      if supported_from < current_version - 1:
        released_platforms[supported_platform] = supported_from
      else:
        rolling_out_platform[supported_platform] = supported_from

    released_platforms = ExpandChromeStar(released_platforms)
    rolling_out_platform = ExpandChromeStar(rolling_out_platform)

    return released_platforms, rolling_out_platform

  def _CheckSingleSchemaValueIsCompatible(self, old_schema_value,
                                          new_schema_value,
                                          custom_value_validation):
    '''
    Checks if a |new_schema_value| in a schema is compatible with an
    |old_schema_value| in a schema. The check will either use the provided
    |custom_value_validation| if any or do a normal equality comparison.
    '''
    return (custom_value_validation == None
            and old_schema_value == new_schema_value) or (
                custom_value_validation != None
                and custom_value_validation(old_schema_value, new_schema_value))

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
        self._SchemaCompatibleError(
            'Value in policy schema path \'%s\' was removed in new schema '
            'value.' % (current_schema_key))
      return

    # Both old and new values must be of the same type.
    if type(old_schema_value) != type(new_schema_value):
      self._SchemaCompatibleError(
          'Value in policy schema path \'%s\' is of type \'%s\' but value in '
          'schema is of type \'%s\'.' %
          (current_schema_key, type(old_schema_value).__name__,
           type(new_schema_value).__name__))

    # We are checking a leaf schema key and do not expect to ever get a
    # dictionary value at this level.
    if (type(old_schema_value) is dict):
      self._SchemaCompatibleError(
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
          self._SchemaCompatibleError(
              'Value \'%s\' in policy schema path \'%s/[%s]\' was added which '
              'is not allowed.' %
              (str(new_schema_value[j]), current_schema_key, j))
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
      self._SchemaCompatibleError(
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
      self._SchemaCompatibleError(
          'Policy schema path \'%s\' in old schema was removed in newer '
          'version.' % (current_schema_key))
      return

    # Both old and new schema information must be in dict format.
    if type(old_schema) is not dict:
      self._SchemaCompatibleError(
          'Policy schema path \'%s\' in old policy is of type \'%s\', it must '
          'be dict type.' % (current_schema_key, type(old_schema)))

    if type(new_schema) is not dict:
      self._SchemaCompatibleError(
          'Policy schema path \'%s\' in new policy is of type \'%s\', it must '
          'be dict type.' % (current_schema_key, type(new_schema)))

    # Both schemas must either have a 'type' key or not. This covers the case
    # where the schema is merely a '$ref'
    if ('type' in old_schema) != ('type' in new_schema):
      self._SchemaCompatibleError(
          'Mismatch in type definition for old schema and new schema for '
          'policy schema path \'%s\'. One schema defines a type while the other'
          ' does not.' %
          (current_schema_key, old_schema['type'], new_schema['type']))
      return

    # For schemas that define a 'type', make sure they match.
    schema_type = None
    if ('type' in old_schema):
      if (old_schema['type'] != new_schema['type']):
        self._SchemaCompatibleError(
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
      # no validation is needed. Anything can be done to it including removal.
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
          self._SchemaCompatibleError(
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
          self._SchemaCompatibleError(
              'Unexpected type \'%s\' at policy schema path \'%s\'. It must be '
              'dict' % (type(old_value).__name__, ))
          continue

        # Make sure that all old properties exist and are compatible. Everything
        # else that is new requires no validation.
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
      self._SchemaCompatibleError(
          'Key \'%s\' was added to policy schema path \'%s\' in new schema.' %
          (new_key, current_schema_key))

  def _CheckPolicyDefinitionChangeCompatibility(
      self, policy_name, original_policy, original_released_platforms,
      new_policy, new_released_platforms, current_version):
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
        self._PolicyError(f'Released platform {platform} has been removed.',
                          new_policy, 'supported_on')
      elif original_released_platforms[platform] < new_released_platforms[
          platform]:
        self._PolicyError(
            'Released policy supported version is changed.\n'
            f'  Supported version of released platform {platform} changed to '
            f'a later version {new_released_platforms[platform]} from '
            f'{original_released_platforms[platform]}.', new_policy,
            'supported_on')

    # 2. Check if the policy has changed its device_only value
    if original_policy.get('device_only', False) != new_policy.get(
        'device_only', False):
      self._PolicyError('Released policy device_only status changed.',
                        new_policy, 'device_only')

    # 3. Check schema changes for compatibility.
    self.schema_compatible_errors = []
    self._CheckSchemasAreCompatible([policy_name], original_policy['schema'],
                                    new_policy['schema'])
    if self.schema_compatible_errors:
      schema_compatible_error_message = '\n  '.join(
          self.schema_compatible_errors)
      self._PolicyError(
          'Schema compatible errors.\n'
          f'  {schema_compatible_error_message}', new_policy)

  def _CheckNewReleasedPlatforms(self, original_platforms, new_platforms,
                                 current_version, policy):
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
            '%d.' %
            (policy.get('name'), platform, new_version, current_version))
      elif new_version < current_version - 1:
        self.non_compatibility_error_count += 1
        self._PolicyError(
            'Released policy supported version is changed.\n'
            f'  Version {new_version} has been released to Stable already. '
            f'Please use version {current_version} instead for platform '
            f'{platform}.', policy, 'supported_on')

  # Checks if the new policy definitions are compatible with the policy
  # definitions coming from the original_file_contents.
  def _CheckPolicyDefinitionsChangeCompatibility(self, policy_change_list,
                                                 current_version):
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
    for policy_changes in policy_change_list:
      original_policy = policy_changes['old_policy']
      new_policy = policy_changes['new_policy']
      if new_policy:
        new_policy['name'] = policy_changes['policy']
      if original_policy:
        original_policy['name'] = policy_changes['policy']
        (original_released_platforms,
         original_rolling_out_platforms) = self._GetReleasedPlatforms(
             original_policy, current_version)

        # A policy that has at least one released platform cannot be removed.
        if not new_policy:
          name = original_policy['name']
          if original_released_platforms:
            self._PolicyError(f'Released policy has been removed.',
                              original_policy)
          else:
            self._Warning(
                f'Unreleased Policy {name} has been removed. If the '
                'policy is available in Beta, please cleanup the Beta '
                'branch as well.')
          continue

        (new_released_platforms,
         new_rolling_out_platform) = self._GetReleasedPlatforms(
             new_policy, current_version)

        # Check policy compatibility if there is at least one released platform.
        if original_released_platforms:
          self._CheckPolicyDefinitionChangeCompatibility(
              new_policy['name'], original_policy, original_released_platforms,
              new_policy, new_released_platforms, current_version)

        # New released platforms should always use the current version unless
        # they are going to be merged into previous milestone.
        if new_released_platforms or new_rolling_out_platform:
          self._CheckNewReleasedPlatforms(
              MergeDict(original_released_platforms,
                        original_rolling_out_platforms),
              MergeDict(new_released_platforms, new_rolling_out_platform),
              current_version, new_policy)
      elif new_policy:
        (new_released_platforms,
         new_rolling_out_platform) = self._GetReleasedPlatforms(
             new_policy, current_version)
        if new_released_platforms or new_rolling_out_platform:
          self._CheckNewReleasedPlatforms({},
                                          MergeDict(new_released_platforms,
                                                    new_rolling_out_platform),
                                          current_version, new_policy)
        # TODO(crbug.com/1139046): This default check should apply to all
        # policies instead of just new ones.
        if self._NeedsDefault(new_policy) and not 'default' in new_policy:
          self._Error("Definition of policy %s must include a 'default'"
                      " field." % (new_policy['name']))

        # TODO(crbug.com/1139306): This item check should apply to all policies
        # instead of just new ones.
        if self._NeedsItems(new_policy) and new_policy.get('items',
                                                           None) == None:
          self._PolicyError('Policy does not have "items" field', new_policy)

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


  def _ValidatePolicyAtomicGroups(self, atomic_groups, max_id, deleted_ids):
    ids = sorted([x['id'] for x in atomic_groups])
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

      if i > 0 and ids[i - 1] + 1 != ids[i]:
        for delete_id in range(ids[i - 1] + 1, ids[i]):
          if delete_id not in deleted_ids:
            self._Error('Missing atomic group id %s' % (delete_id))
            return

  def Main(self, legacy_policy_template, options, policy_change_list,
           current_version, skip_compability_check):
    self.options = options

    # First part: check structure.

    # Check (non-policy-specific) message definitions.
    messages = self._CheckContains(legacy_policy_template,
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
    policy_definitions = self._CheckContains(legacy_policy_template,
                                             'policy_definitions',
                                             list,
                                             parent_element=None,
                                             container_name='The root element',
                                             offending=None)
    deleted_policy_ids = self._CheckContains(legacy_policy_template,
                                             'deleted_policy_ids',
                                             list,
                                             parent_element=None,
                                             container_name='The root element',
                                             offending=None)
    deleted_atomic_policy_group_ids = self._CheckContains(
        legacy_policy_template,
        'deleted_atomic_policy_group_ids',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)
    highest_atomic_group_id = self._CheckContains(
        legacy_policy_template,
        'highest_atomic_group_id_currently_used',
        int,
        parent_element=None,
        container_name='The root element',
        offending=None)
    device_policy_proto_map = self._CheckContains(
        legacy_policy_template,
        'device_policy_proto_map',
        dict,
        parent_element=None,
        container_name='The root element',
        offending=None)
    legacy_device_policy_proto_map = self._CheckContains(
        legacy_policy_template,
        'legacy_device_policy_proto_map',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)
    policy_atomic_group_definitions = self._CheckContains(
        legacy_policy_template,
        'policy_atomic_group_definitions',
        list,
        parent_element=None,
        container_name='The root element',
        offending=None)

    self._ValidatePolicyAtomicGroups(policy_atomic_group_definitions,
                                     highest_atomic_group_id,
                                     deleted_atomic_policy_group_ids)
    self._CheckDevicePolicyProtoMappingUniqueness(
        device_policy_proto_map, legacy_device_policy_proto_map)
    self._CheckDevicePolicyProtoMappingExistence(
        device_policy_proto_map, options.device_policy_proto_path)

    if policy_definitions is not None:
      for policy in policy_definitions:
        self._CheckDevicePolicyProtoMappingDeviceOnly(
            policy, device_policy_proto_map, legacy_device_policy_proto_map)
      self._CheckTotalDevicePolicyExternalDataMaxSize(policy_definitions)

    # Made it as a dict (policy_name -> True) to reuse _CheckContains.
    policy_names = {
        policy['name']: True
        for policy in policy_definitions
        if self.policy_type_provider.GetPolicyType(policy) != 'group'
    }
    policy_in_groups = set()
    for group in [
        policy for policy in policy_definitions
        if self.policy_type_provider.GetPolicyType(policy) == 'group'
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
    # TODO(crbug/1375858): Check valid yaml formatting

    # Third part: summary and exit.
    if self.options.stats:
      if self.num_groups > 0:
        print('%d policies, %d of those in %d groups (containing on '
              'average %.1f policies).' %
              (self.num_policies, self.num_policies_in_groups, self.num_groups,
               (1.0 * self.num_policies_in_groups / self.num_groups)))
      else:
        print(self.num_policies, 'policies, 0 policy groups.')
    return

  def CheckModifiedPolicies(self, policy_change_list, current_version,
                            skip_compability_check, known_features,
                            schemas_by_id):
    '''
      Checks that changes made to policies `policy_change_list` are compatible
      with the `current_version` and previous versions of the policy.
      This also check that the policy definition schema matches the expected
      schema for a policy. `skip_compability_check` is used to skip the schema
      and version compatibility checks and must be used with care.
      'known_features' is a list of faetures that we can find in the feature
      list for policies.
      Returns warnings and errors found in the policies.
    '''
    self.features = known_features
    modified_policies = [
        pc['new_policy'] for pc in policy_change_list
        if pc['new_policy'] is not None
    ]
    for policy in modified_policies:
      self._CheckPolicyDefinition(policy, current_version, schemas_by_id)
    self.non_compatibility_error_count = 0
    if (not self.errors and not skip_compability_check):
      self._CheckPolicyDefinitionsChangeCompatibility(policy_change_list,
                                                      current_version)

    if self.non_compatibility_error_count > 0:
      print(
          '\nThere were compatibility validation errors in the change. You may '
          'bypass this validation by adding "BYPASS_POLICY_COMPATIBILITY_CHECK='
          '<justification>" to your changelist description. If you believe '
          'that this validation is a bug, please file a crbug against '
          '"Enterprise" and add a link to the bug as '
          'justification. Otherwise, please provide an explanation for the '
          'change. For more information please refer to: '
          'https://bit.ly/33qr3ZV.')
    return self.errors, self.warnings

  def Run(self,
          argv,
          legacy_policy_template=None,
          policy_change_list=[],
          current_version=None,
          skip_compability_check=False):
    parser = argparse.ArgumentParser(
        usage='usage: %prog [options] template_dir',
        description='Syntax check a generated policy_templates.json file.')
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
    if legacy_policy_template is None:
      self._Error('Template not specified.')
      return self.errors, self.warnings
    if args.device_policy_proto_path is None:
      self._Error('Missing --device_policy_proto_path argument.')
      return self.errors, self.warnings
    self.Main(legacy_policy_template, args, policy_change_list, current_version,
              skip_compability_check)
    return self.errors, self.warnings

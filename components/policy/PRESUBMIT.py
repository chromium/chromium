# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this presubmit check fails or misbehaves, please complain to
# chromium-policy-owners@google.com.

PRESUBMIT_VERSION = '2.0.0'

import glob
import os
import sys
from xml.dom import minidom
from xml.parsers import expat

sys.path.append(os.path.abspath('./resources'))
from policy_templates import GetPolicyTemplates

sys.path.append(os.path.join('..', '..', 'third_party'))
import pyyaml


_CACHED_FILES = {}
_CACHED_POLICY_CHANGE_LIST = []
_CACHED_POLICY_DEFINITION_MAP = {}

_COMPONENTS_POLICY_PATH = os.path.join('components', 'policy')
_TEST_CASES_DEPOT_PATH = os.path.join(
    _COMPONENTS_POLICY_PATH, 'test' , 'data', 'pref_mapping')
_PRESUBMIT_PATH = os.path.join(_COMPONENTS_POLICY_PATH, 'PRESUBMIT.py')
_TOOLS_PATH = os.path.join(_COMPONENTS_POLICY_PATH, 'tools')
_SYNTAX_CHECK_SCRIPT_PATH = os.path.join(_TOOLS_PATH,
      'syntax_check_policy_template_json.py')
_TEMPLATES_PATH = os.path.join(_COMPONENTS_POLICY_PATH, 'resources',
      'templates')
_MESSAGES_PATH = os.path.join(_TEMPLATES_PATH, 'messages.yaml')
_COMMON_SCHEMAS_PATH = os.path.join(_TEMPLATES_PATH, 'common_schemas.yaml')
_POLICIES_DEFINITIONS_PATH = os.path.join(_TEMPLATES_PATH, 'policy_definitions')
_POLICIES_YAML_PATH = os.path.join(_TEMPLATES_PATH, 'policies.yaml')
_ENUMS_PATH = os.path.join(
      'tools', 'metrics', 'histograms', 'metadata', 'enterprise', 'enums.xml')
_DEVICE_POLICY_PROTO_PATH = os.path.join(
      _COMPONENTS_POLICY_PATH, 'proto', 'chrome_device_policy.proto')
_DEVICE_POLICY_PROTO_MAP_PATH = os.path.join(
      _TEMPLATES_PATH, 'manual_device_policy_proto_map.yaml')
_LEGACY_DEVICE_POLICY_PROTO_MAP_PATH = os.path.join(
      _TEMPLATES_PATH, 'legacy_device_policy_proto_map.yaml')


# 100 MiB upper limit on the total device policy external data max size limits
# due to the security reasons.
# You can increase this limit if you're introducing new external data type
# device policy, but be aware that too heavy policies could result in user
# profiles not having enough space on the device.
TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE = 1024 * 1024 * 100


def _SafeListDir(directory):
  '''Wrapper around os.listdir() that ignores files created by Finder.app.'''
  # On macOS, Finder.app creates .DS_Store files when a user visit a
  # directory causing failure of the script laters on because there
  # are no such group as .DS_Store. Skip the file to prevent the error.
  return filter(lambda name:(name != '.DS_Store'),os.listdir(directory))


def _SkipPresubmitChecks(input_api, files_watchlist):
  '''Returns True if no file or file under the directories specified was
     affected in this change.
     Args:
       input_api
       files_watchlist: List of files or directories
  '''
  for file in files_watchlist:
    if any(os.path.commonpath([file, f.LocalPath()]) == file for f in
           input_api.change.AffectedFiles()):
      return False

  return True

def _CheckerWasModified(input_api):
  '''Returns True if the syntax checker file was modified.
     Args:
       input_api
  '''
  return any(_SYNTAX_CHECK_SCRIPT_PATH == f for f in
             input_api.change.LocalPaths())


def _LoadYamlFile(root, path):
  str_path = str(path)
  if str_path not in _CACHED_FILES:
    with open(os.path.join(root, path), encoding='utf-8') as f:
      _CACHED_FILES[str_path] = pyyaml.safe_load(f)
  return _CACHED_FILES[str_path]


def _GetKnownFeatures(input_api):
  feature_messages = []
  root = input_api.change.RepositoryRoot()
  messages = _LoadYamlFile(root, _MESSAGES_PATH)
  for message in messages:
    if message.startswith('doc_feature_'):
      feature_messages.append(message[12:])
  return feature_messages


def _GetCommonSchema(input_api):
  root = input_api.change.RepositoryRoot()
  commmon_schemas = _LoadYamlFile(root, _COMMON_SCHEMAS_PATH)
  return commmon_schemas


def _GetCurrentVersion(input_api):
  if 'version' in _CACHED_FILES:
    return _CACHED_FILES['version']
  try:
    root = input_api.change.RepositoryRoot()
    version_path = input_api.os_path.join(root, 'chrome', 'VERSION')
    with open(version_path, "rb") as f:
      _CACHED_FILES['version'] = int(f.readline().split(b"=")[1])
  except:
    pass
  return _CACHED_FILES['version']


def _GetPolicyDefinitionMap(input_api):
  '''Returns a dict of policy definitions as they are in this changelist.
     Args:
       input_api
     Returns:
       Dictionary of policies loaded from their yaml files with the policy name
       as the key.
  '''
  global _CACHED_POLICY_DEFINITION_MAP
  if not _CACHED_POLICY_DEFINITION_MAP:
    policy_definitions = GetPolicyTemplates()['policy_definitions']
    _CACHED_POLICY_DEFINITION_MAP = \
        {policy['name']: policy for policy in policy_definitions}

  return _CACHED_POLICY_DEFINITION_MAP


def _GetUnchangedPolicyList(input_api):
  '''Returns a list of policies NOT modified in the changelist
     Args:
       input_api
     Returns:
       The list of policies loaded from their yaml files with the 'name' added.
  '''
  changed_policy_names = {
      policy['policy'] for policy in _GetPolicyChangeList(input_api)
  }
  root = input_api.change.RepositoryRoot()
  policies_dir = input_api.os_path.join(root,
                                        _POLICIES_DEFINITIONS_PATH)
  results = []
  for path in glob.iglob(policies_dir + '/**/*.yaml', recursive=True):
    filename = os.path.basename(path)
    if not filename.endswith(".yaml"):
      continue;
    if (filename == '.group.details.yaml' or
        filename == 'policy_atomic_groups.yaml'):
      continue
    policy_name = filename.partition('.')[0]
    if policy_name in changed_policy_names:
      continue;
    policy = _LoadYamlFile('/', path)
    policy['name'] = policy_name
    results.append(policy)
  return results


def _GetPolicyChangeList(input_api):
  '''Returns a list of policies modified in the changelist with their old schema
     next to their new schemas.
     Args:
       input_api
     Returns:
       List of objects with the following schema:
       { 'name': 'string', 'old_policy': dict, 'new_policy': dict }
       The policies are the values loaded from their yaml files.
  '''
  if _CACHED_POLICY_CHANGE_LIST:
    return _CACHED_POLICY_CHANGE_LIST

  policy_changes_map = {}
  root = input_api.change.RepositoryRoot()
  policies_dir = input_api.os_path.join(root,
                                        _POLICIES_DEFINITIONS_PATH)
  policy_name_to_id = {name: id
    for id, name
    in _LoadYamlFile(root, _POLICIES_YAML_PATH)['policies'].items()}
  template_affected_files = [f for f in input_api.change.AffectedFiles()
    if os.path.commonpath([policies_dir,
      f.AbsoluteLocalPath()]) ==  policies_dir]

  for affected_file in template_affected_files:
    path = affected_file.AbsoluteLocalPath()
    filename = os.path.basename(path)
    policy_name = os.path.splitext(filename)[0]
    if (filename == '.group.details.yaml' or
        filename == 'policy_atomic_groups.yaml' or
        filename == 'OWNERS' or
        filename == 'DIR_METADATA'):
      continue

    if policy_name not in policy_name_to_id and affected_file.Action() != 'D':
      raise Exception("Policy not listed in %s: '%s'" % (
          _POLICIES_YAML_PATH, policy_name))

    old_policy = None
    new_policy = None
    if affected_file.Action() == 'M':
      old_policy = pyyaml.safe_load('\n'.join(affected_file.OldContents()))
      old_policy['name'] = policy_name
      old_policy['id'] = policy_name_to_id[policy_name]

    if affected_file.Action() == 'D':
      old_policy = pyyaml.safe_load('\n'.join(affected_file.OldContents()))
      old_policy['name'] = policy_name

    if affected_file.Action() != 'D':
      new_policy = pyyaml.safe_load('\n'.join(affected_file.NewContents()))
      new_policy['name'] = policy_name
      new_policy['id'] = policy_name_to_id[policy_name]

    # If a policy has been moved, it will appear as deleted then added.
    # Here we reconcile such policies so that a moved policy does not appear as
    # deleted. This also allows to verify the new policy schema against the one
    # from the previous location.
    if policy_name in policy_changes_map:
      # We previously found the policy at the new location, update old_policy
      # with the value from the old location.
      if policy_changes_map[policy_name]['old_policy'] == None:
        policy_changes_map[policy_name]['old_policy'] = old_policy
      # We previously found the policy at the old location, update new_policy
      # with the value from the new location.
      if policy_changes_map[policy_name]['new_policy'] == None:
        policy_changes_map[policy_name]['new_policy'] = new_policy
    else:
      policy_changes_map[policy_name] = {
      'policy': policy_name,
      'old_policy': old_policy,
      'new_policy': new_policy}

  for policy_change in policy_changes_map.values():
    _CACHED_POLICY_CHANGE_LIST.append(policy_change)

  return _CACHED_POLICY_CHANGE_LIST


def _IsPolicyUnsupported(input_api, policy):
  '''Returns true if `policy` is unsupported on the current Chrome version on
     all platforms. These policies may not have any prefs and tests associated
     with them.'''
  if len(policy.get('future_on', [])) > 0:
    # If the policy will be released in the future, it is supported.
    return False

  current_version = _GetCurrentVersion(input_api)
  policy_platforms = _GetPlatformSupportMap(policy)
  for _, supported_versions in policy_platforms.items():
    if not supported_versions['to']:
      # Policy doesn't have an end of support version.
      return False

    if supported_versions['to'] >= current_version:
      return False

  return True


def CheckPolicyTestCases(input_api, output_api):
  '''Verifies that the all defined policies have a test case.
  This is ran when policy_test_cases.json, policies.yaml or this PRESUBMIT.py
  file are modified.
  '''
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_TEST_CASES_DEPOT_PATH, _POLICIES_YAML_PATH, _POLICIES_DEFINITIONS_PATH,
       _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()

  # Gather expected test files
  policies_yaml = _LoadYamlFile(root, _POLICIES_YAML_PATH)
  policies = policies_yaml['policies']
  policy_names = set(name for name in policies.values() if name)

  test_case_depot_path = os.path.join(
    root, _TEST_CASES_DEPOT_PATH)

  # Gather actual test files
  tested_policies = set()
  for file in _SafeListDir(test_case_depot_path):
    filename = os.fsdecode(file)
    policy_name = os.path.splitext(filename)[0]
    tested_policies.add(policy_name)

  # Finally check if any policies or tests are missing.
  policies_with_missing_tests = policy_names - tested_policies
  extra = tested_policies - policy_names
  error_missing = ("Policy '%s' is declared but its test file '%s' was not "
                  "found. Please update the test accordingly.")
  error_extra = ("Policy '%s' is tested at '%s' but its policy definition was "
                 "not found. Please update the policy definition accordingly.")
  results = []
  for policy in policies_with_missing_tests:
    policy_definition = _GetPolicyDefinitionMap(input_api).get(policy, {})
    if _IsPolicyUnsupported(input_api, policy_definition):
      # Unsupported policies won't have tests.
      continue
    results.append(output_api.PresubmitError(
      error_missing % (
        policy, os.path.join(test_case_depot_path, f'{policy}.json'))))
  for policy in extra:
    results.append(output_api.PresubmitError(
      error_extra % (
        policy, os.path.join(test_case_depot_path, f'{policy}.json'))))

  results.extend(
      input_api.canned_checks.CheckChangeHasNoTabs(
          input_api,
          output_api,
          source_file_filter=lambda x: x.LocalPath() == _TEST_CASES_DEPOT_PATH))

  return results


def CheckPolicyHistograms(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_ENUMS_PATH, _POLICIES_YAML_PATH, _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()

  with open(os.path.join(root, _ENUMS_PATH), encoding='utf-8') as f:
    tree = minidom.parseString(f.read())
  enums = (tree.getElementsByTagName('histogram-configuration')[0]
               .getElementsByTagName('enums')[0]
               .getElementsByTagName('enum'))
  policy_enum = [e for e in enums
                 if e.getAttribute('name') == 'EnterprisePolicies'][0]
  policy_enum_ids = frozenset(int(e.getAttribute('value'))
                              for e in policy_enum.getElementsByTagName('int'))
  policies_yaml = _LoadYamlFile(root, _POLICIES_YAML_PATH)
  policies = policies_yaml['policies']
  policy_ids = frozenset([id for id, name in policies.items() if name])

  missing_ids = policy_ids - policy_enum_ids
  extra_ids = policy_enum_ids - policy_ids

  error_common = ("To regenerate the policy part of enums.xml, run:\n"
                  "python3 tools/metrics/histograms/update_policies.py")
  error_missing = (f"Policy '%s' (id %d) was added to policy_templates.json "
                   f"but not to {_ENUMS_PATH}. Please update both files. "
                   f"{error_common}")
  error_extra = (f"Policy id %d was found in {_ENUMS_PATH}, but no policy with "
                 f"this id exists in policy_templates.json. {error_common}")
  results = []
  for policy_id in missing_ids:
    results.append(
        output_api.PresubmitError(error_missing %
                                  (policies[policy_id], policy_id)))
  for policy_id in extra_ids:
    results.append(output_api.PresubmitError(error_extra % policy_id))
  return results


def CheckMessages(input_api, output_api):
  '''Verifies that the all the messages from messages.yaml have the following
  format: {[key: string]: {text: string, desc: string}}.
  This is ran when messages.yaml or this PRESUBMIT.py
  file are modified.
  '''
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_MESSAGES_PATH, _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()
  messages = _LoadYamlFile(root, _MESSAGES_PATH)

  for message in messages:
    # |key| must be a string, |value| a dict.
    if not isinstance(message, str):
      results.append(
        output_api.PresubmitError(
          f'Each message key must be a string, invalid key {message}'))
      continue

    if not isinstance(messages[message], dict):
      results.append(
        output_api.PresubmitError(
          f'Each message must be a dictionary, invalid message {message}'))
      continue

    if ('desc' not in messages[message] or
        not isinstance(messages[message]['desc'], str)):
      results.append(
        output_api.PresubmitError(
          f"'desc' string key missing in message {message}"))

    if ('text' not in messages[message] or
        not isinstance(messages[message]['text'], str)):
      results.append(
        output_api.PresubmitError(
          f"'text' string key missing in message {message}"))

    # There should not be any unknown keys in |value|.
    for vkey in messages[message]:
      if vkey not in ('desc', 'text'):
        results.append(output_api.PresubmitError(
          f'In message {message}: Unknown key: {vkey}'))
  return results


def CheckMissingPlaceholders(input_api, output_api):
  '''Verifies that the all the messages from messages.yaml, caption and
  descriptions from files under templates/policy_definitions do not have
  malformed placeholders.
  This is ran when messages.yaml, files under templates/policy_definitions or
  this PRESUBMIT.py file are modified.
  '''
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_MESSAGES_PATH, _POLICIES_DEFINITIONS_PATH, _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()
  new_policies = [change['new_policy']
    for change in _GetPolicyChangeList(input_api)]
  messages = _LoadYamlFile(root, _MESSAGES_PATH)
  items = new_policies + list(messages.values())
  for item in items:
    for key in ['desc', 'text']:
      if item is None:
        continue
      if not key in item:
        continue
      try:
        node = minidom.parseString(u'<msg>%s</msg>' % item[key]).childNodes[0]
      except expat.ExpatError as e:
        error = (
            'Error when checking for missing placeholders: %s in:\n'
            '!<Policy Start>!\n%s\n<Policy End>!' %
            (e, item[key]))
        results.append(output_api.PresubmitError(error))
        continue

      for child in node.childNodes:
        if child.nodeType == minidom.Node.TEXT_NODE and '$' in child.data:
          warning = ("Character '$' found outside of a placeholder in '%s'. "
                     "Should it be in a placeholder ?") % item[key]
          results.append(output_api.PresubmitPromptWarning(warning))
  return results


def CheckDevicePolicyProtos(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_DEVICE_POLICY_PROTO_PATH, _DEVICE_POLICY_PROTO_MAP_PATH,
       _LEGACY_DEVICE_POLICY_PROTO_MAP_PATH, _PRESUBMIT_PATH]):
    return results
  root = input_api.change.RepositoryRoot()

  proto_map = _LoadYamlFile(root, _DEVICE_POLICY_PROTO_MAP_PATH)
  legacy_proto_map = _LoadYamlFile(root, _LEGACY_DEVICE_POLICY_PROTO_MAP_PATH)
  with open(os.path.join(root, _DEVICE_POLICY_PROTO_PATH),
            'r', encoding='utf-8') as file:
    protos = file.read()
  results = []
  # Check that proto_map does not have duplicate values.
  proto_paths = set()
  for proto_path in proto_map.values():
    if proto_path in proto_paths:
      results.append(output_api.PresubmitError(
          f'Duplicate proto path {proto_path} in '
          f'{os.path.basename(_DEVICE_POLICY_PROTO_MAP_PATH)}. '
          'Did you set the right path for your device policy?'))
    proto_paths.add(proto_path)

  # Check that legacy_proto_map does not have duplicate values.
  for proto_path_list in legacy_proto_map.values():
    for proto_path in proto_path_list:
      if not proto_path:
        continue
      if proto_path in proto_paths:
        results.append(output_api.PresubmitError(
          f'Duplicate proto path {proto_path} in '
          'legacy_device_policy_proto_map.yaml.'
          'Did you set the right path for your device policy?'))
      proto_paths.add(proto_path)

  for policy, proto_path in proto_map.items():
    fields = proto_path.split(".")
    for field in fields:
      if field not in protos:
        results.append(output_api.PresubmitError(
         f"Policy '{policy}': Expected field '{field}' not found in "
         "chrome_device_policy.proto."))
  return results


def _GetPlatformSupportMap(policy):
  '''Returns a map of platforms to their support version range as an object
     with the keys `from` and `to`.'''
  platforms_and_versions = {}
  if not policy:
    return platforms_and_versions
  for supported_on in policy.get('supported_on', []):
    platform, versions = supported_on.split(':')
    supported_from, supported_to = versions.split('-')
    version_range = {
      'from': int(supported_from) if supported_from else None,
      'to': int(supported_to) if supported_to else None
    }
    if platform == 'chrome.*':
      for p in ['chrome.win', 'chrome.mac', 'chrome.linux']:
        platforms_and_versions[p] = version_range
    else:
      platforms_and_versions[platform] = version_range
  return platforms_and_versions


def CheckPolicyChangeVersionPlatformCompatibility(input_api, output_api):
  '''Cheks if the modified policies are compatible with their previous version
    if any and if they are compatible with the current version.

    Args:
    policy_changelist: A list of changed policy definitions with their old and
                         new values.
    original_file_contents: The full contents of the original policy templates
      file.
    current_version: The current major version of the branch as stored in
      chrome/VERSION.'''
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_POLICIES_DEFINITIONS_PATH, _PRESUBMIT_PATH]):
    return results

  skip_compatibility_check = ('BYPASS_POLICY_COMPATIBILITY_CHECK'
                                in input_api.change.tags)
  if skip_compatibility_check:
    return results

  policy_changelist = _GetPolicyChangeList(input_api)
  current_version = _GetCurrentVersion(input_api)
  for policy_changes in policy_changelist:
    original_policy = policy_changes['old_policy']
    new_policy = policy_changes['new_policy']
    policy_name = policy_changes['policy']
    original_policy_platforms = _GetPlatformSupportMap(original_policy)
    new_policy_platforms = _GetPlatformSupportMap(new_policy)

    for platform, original_range in original_policy_platforms.items():
      # Policy supported
      if original_range['from'] < current_version:
        if platform not in new_policy_platforms:
          results.append(output_api.PresubmitError(
            f"In policy {policy_name}: Policy has been removed on {platform}. "
            "A released policy cannot be removed. Mark it as deprecated and "
            "update the supported versions."))

      if original_range['from'] >= current_version:
        if platform not in new_policy_platforms:
          results.append(output_api.PresubmitPromptWarning(
            f"Unreleased policy {policy_name} has been removed on {platform}."))

    for platform, _ in new_policy_platforms.items():
      new_from_version = new_policy_platforms[platform]['from']
      if (new_from_version < current_version - 1 and
          platform not in original_policy_platforms):
        results.append(output_api.PresubmitError(
          f"In policy {policy_name}: Support can't be added on platform "
          f"{platform} because version {new_from_version} is already released.")
        )

      if (new_from_version == current_version - 1 and
          platform not in original_policy_platforms):
        results.append(output_api.PresubmitPromptWarning(
          f"In policy {policy_name}: Support will be added on platform "
          f"{platform} version {new_from_version} which has already passed "
          "branch point. Please merge this change in Beta."))

      if not new_policy_platforms[platform]['to']:
        continue
      # These warnings fire inappropriately in presubmit --all/--files runs, so
      # disable them in these cases to reduce the noise.
      if input_api.no_diffs:
        continue
      # An end-milestone for policies can only be added for versions that have
      # already branched, until we have a better reminder process to cleanup
      # the code related to deprecated policies.
      end_version = new_policy_platforms[platform]['to']
      if end_version >= current_version:
        results.append(output_api.PresubmitPromptWarning(
          f"In policy {policy_name} for platform {platform}: An end-milestone "
          f"of {end_version} was used. But policies are only allowed to be end-"
          f"dated at versions that have already branched, currently "
          f"M{current_version - 1} or before. Please remove all references in "
          f"the code to {end_version}, and instead file a bug with a reminder "
          f"to add the end milestone after M{end_version - 1} branches."))
  return results


def CheckMissingPolicyNames(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_MESSAGES_PATH, _POLICIES_DEFINITIONS_PATH, _SYNTAX_CHECK_SCRIPT_PATH,
       _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()

  # Check for missing policy names in policy.yaml and policy names to be removed
  # from policy.yaml.
  policies_yaml = _LoadYamlFile(root, _POLICIES_YAML_PATH)
  policies = policies_yaml['policies']
  policy_names = frozenset([name for _, name in policies.items() if name])
  policy_changelist = _GetPolicyChangeList(input_api)
  for policy_change in policy_changelist:
    policy_name = policy_change['policy']
    if policy_change['new_policy'] and policy_name not in policy_names:
      results.append(output_api.PresubmitError(
            f'{policy_name} needs an ID in {_POLICIES_YAML_PATH}'))
    if not policy_change['new_policy'] and policy_name in policy_names:
      results.append(output_api.PresubmitError(
            f'{policy_name}\'s needs to be erased from {_POLICIES_YAML_PATH}'))

  return results


def CheckPoliciesYamlOrdering(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_POLICIES_YAML_PATH, _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()
  with open(os.path.join(root, _POLICIES_YAML_PATH),
            'r', encoding='utf-8') as f:
    policies_yaml_lines = f.readlines()

  previous_id = 0
  error_msg_template = ''
  for line in policies_yaml_lines:
    if line.startswith('  '):
      if not error_msg_template:
        results.append(output_api.PresubmitError(
          f'Invalid syntax, missing either policies, or atomic_groups key.'))
        continue
      id = int(line.strip().split(':')[0])
      if previous_id + 1 != id:
        results.append(output_api.PresubmitError(error_msg_template % id))
      previous_id = id
    elif 'policies:' in line:
      error_msg_template = 'Policy ID %s is out of place'
      previous_id = 0
    elif  'atomic_groups:' in line:
      error_msg_template = 'Atomic policy group ID %s is out of place'
      previous_id = 0
  return results


def CheckPolicyIds(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_MESSAGES_PATH, _POLICIES_DEFINITIONS_PATH, _SYNTAX_CHECK_SCRIPT_PATH,
       _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()

  # Check for duplicated ids
  policies_yaml = _LoadYamlFile(root, _POLICIES_YAML_PATH)
  policies = policies_yaml['policies']
  policy_ids = set()
  duplicated_policy_ids = []
  for id, _ in policies.items():
    if id in policy_ids:
      duplicated_policy_ids.add(id)
    policy_ids.add(id)

  if duplicated_policy_ids:
    duplicated_policy_ids_str = ', '.join(duplicated_policy_ids)
    results.append(output_api.PresubmitError(
        f'Duplicate ids {duplicated_policy_ids_str} in {_POLICIES_YAML_PATH}'))

  # Check for missing ids
  missing_ids = sorted(list(set(range(1, max(policy_ids) + 1)) - policy_ids))
  if missing_ids:
    missing_ids_str = ', '.join(str(id) for id in missing_ids)
    results.append(output_api.PresubmitError(
        f'Missing policy ids {missing_ids_str} in {_POLICIES_YAML_PATH}'))

  return results



def CheckPolicyDefinitions(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_MESSAGES_PATH, _POLICIES_DEFINITIONS_PATH, _SYNTAX_CHECK_SCRIPT_PATH,
       _COMMON_SCHEMAS_PATH, _PRESUBMIT_PATH]):
    return results

  # Get the current version from the VERSION file so that we can check
  # which policies are un-released and thus can be changed at will.
  current_version = _GetCurrentVersion(input_api)

  old_sys_path = sys.path
  tools_path = input_api.os_path.normpath(input_api.os_path.join(
    input_api.PresubmitLocalPath(), 'tools'))
  sys.path.append(tools_path)
  # Optimization: only load this when it's needed.
  import syntax_check_policy_template_json
  sys.path = old_sys_path

  schemas_by_id = _GetCommonSchema(input_api)
  checker = syntax_check_policy_template_json.PolicyTemplateChecker()
  checker.SetFeatures(_GetKnownFeatures(input_api))
  # Check if there is a tag that allows us to bypass compatibility checks.
  # This can be used in situations where there is a bug in the validation
  # code or if a policy change needs to urgently be submitted.
  skip_compatibility_check = ('BYPASS_POLICY_COMPATIBILITY_CHECK'
                                in input_api.change.tags)
  checker.CheckModifiedPolicies(
    _GetPolicyChangeList(input_api), current_version,
    schemas_by_id, skip_compatibility_check)

  if _CheckerWasModified(input_api):
    # Check the rest of the policies
    checker.CheckPolicyDefinitions(_GetUnchangedPolicyList(input_api),
                                   current_version,
                                   schemas_by_id)

  errors, warnings = checker.errors, checker.warnings

  # PRESUBMIT won't print warning if there is any error. Append warnings to
  # error for policy_templates.json so that they can always be printed
  # together.
  if errors:
    error_msgs = "\n".join(errors+warnings)
    return [output_api.PresubmitError('Syntax error(s) in file:',
                                      [_TEMPLATES_PATH],
                                      error_msgs)]
  elif warnings:
    warning_msgs = "\n".join(warnings)
    return [output_api.PresubmitPromptWarning('Syntax warning(s) in file:',
                                                [_TEMPLATES_PATH],
                                                warning_msgs)]

  return []


def CheckDevicePolicies(input_api, output_api):
  results = []
  if _SkipPresubmitChecks(
      input_api,
      [_POLICIES_DEFINITIONS_PATH, _PRESUBMIT_PATH]):
    return results

  root = input_api.change.RepositoryRoot()
  policy_changelist = _GetPolicyChangeList(input_api)
  if not any(policy_change['new_policy'].get('device_only', False)
             or policy_change['new_policy']['type'] == 'external'
             for policy_change in policy_changelist
             if policy_change['new_policy'] != None):
    return results

  policy_definitions = list(_GetPolicyDefinitionMap(input_api).values())

  proto_map = _LoadYamlFile(root, _DEVICE_POLICY_PROTO_MAP_PATH)
  legacy_proto_map = _LoadYamlFile(root, _LEGACY_DEVICE_POLICY_PROTO_MAP_PATH)

  # Check policy did not change its device_only value
  for policy_change in policy_changelist:
    old_policy = policy_change['old_policy']
    new_policy = policy_change['new_policy']
    policy = policy_change['policy']
    if (old_policy and new_policy and
        old_policy.get('device_only', False) !=
        new_policy.get('device_only', False)):
      results.append(output_api.PresubmitError(
        f'In policy {policy}: Released policy device_only status changed.'))

  # Check device policies have a proto mapping
  for policy in policy_definitions:
    if not policy.get('device_only', False):
      continue

    policy_name = policy['name']
    if policy.get('generate_device_proto', True):
      if policy_name in proto_map or policy_name in legacy_proto_map:
        results.append(output_api.PresubmitError(
          f"'{policy_name}' generates the path to the proto. "
          "Please remove it from *_device_policy_proto_map.yaml"))
    else:
      if (policy_name not in proto_map and
          policy_name not in legacy_proto_map):
        results.append(output_api.PresubmitError(
            f"Please set generate_device_proto to true in '{policy_name}.yaml "
            "or add a mapping in manual_device_policy_proto_map.yaml '"))

  # Check that the proto field is equal to the policy name for new policies
  for policy_change in policy_changelist:
    if not policy_change['new_policy'].get('device_only', False):
      continue
    if ('old_policy' in policy_change and
        policy_change['old_policy'] is not None):
      # Ignore existing policies
      continue
    if policy.get('generate_device_proto', True):
      # Ignore policies which will be generated automatically
      continue
    policy_name = policy_change['policy']

    field_name = policy_name + ".value"

    if proto_map[policy_name] != field_name:
      results.append(output_api.PresubmitError(
        f"The proto field in chrome_device_policy.proto for '{policy_name}' "
        "must equal the policy name itself."))

  # Check external data max size
  total_device_policy_external_data_max_size = 0
  for policy in policy_definitions:
    policy_name = policy['name']
    if (policy.get('device_only', False) and policy['type'] == 'external'):
      total_device_policy_external_data_max_size += policy['max_size']
  if (total_device_policy_external_data_max_size >
      TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE):
    results.append(output_api.PresubmitError(
      'Total sum of device policy external data maximum size limits should not '
      f'exceed {TOTAL_DEVICE_POLICY_EXTERNAL_DATA_MAX_SIZE} bytes, current sum '
      f'is {total_device_policy_external_data_max_size} bytes.'))
  return results

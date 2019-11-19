# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this presubmit check fails or misbehaves, please complain to
# mnissler@chromium.org, bartfab@chromium.org or atwilson@chromium.org.

import sys
from xml.dom import minidom

def _GetPolicyTemplates(template_path):
  # Read list of policies in the template. eval() is used instead of a JSON
  # parser because policy_templates.json is not quite JSON, and uses some
  # python features such as #-comments and '''strings'''. policy_templates.json
  # is actually maintained as a python dictionary.
  with open(template_path) as f:
    template_data = eval(f.read(), {})
  policies = [ policy
               for policy in template_data['policy_definitions']
               if policy['type'] != 'group' ]
  return policies

def _CheckPolicyTemplatesSyntax(input_api, output_api):
  local_path = input_api.PresubmitLocalPath()
  filepath = input_api.os_path.join(local_path, 'policy_templates.json')
  if any(f.AbsoluteLocalPath() == filepath
         for f in input_api.AffectedFiles()):
    old_sys_path = sys.path
    try:
      tools_path = input_api.os_path.normpath(
          input_api.os_path.join(local_path, input_api.os_path.pardir, 'tools'))
      sys.path = [ tools_path ] + sys.path
      # Optimization: only load this when it's needed.
      import syntax_check_policy_template_json
      device_policy_proto_path = input_api.os_path.join(
          local_path, '../proto/chrome_device_policy.proto')
      args = ["--device_policy_proto_path=" + device_policy_proto_path]
      checker = syntax_check_policy_template_json.PolicyTemplateChecker()
      if checker.Run(args, filepath) > 0:
        return [output_api.PresubmitError('Syntax error(s) in file:',
                                          [filepath])]
    finally:
      sys.path = old_sys_path
  return []


def _CheckPolicyTestCases(input_api, output_api, policies):
  # Read list of policies in chrome/test/data/policy/policy_test_cases.json.
  root = input_api.change.RepositoryRoot()
  policy_test_cases_file = input_api.os_path.join(
      root, 'chrome', 'test', 'data', 'policy', 'policy_test_cases.json')
  test_names = input_api.json.load(open(policy_test_cases_file)).keys()
  tested_policies = frozenset(name.partition('.')[0]
                              for name in test_names
                              if name[:2] != '--')
  policy_names = frozenset(policy['name'] for policy in policies)

  # Finally check if any policies are missing.
  missing = policy_names - tested_policies
  extra = tested_policies - policy_names
  error_missing = ('Policy \'%s\' was added to policy_templates.json but not '
                   'to src/chrome/test/data/policy/policy_test_cases.json. '
                   'Please update both files.')
  error_extra = ('Policy \'%s\' is tested by '
                 'src/chrome/test/data/policy/policy_test_cases.json but is not'
                 ' defined in policy_templates.json. Please update both files.')
  results = []
  for policy in missing:
    results.append(output_api.PresubmitError(error_missing % policy))
  for policy in extra:
    results.append(output_api.PresubmitError(error_extra % policy))
  return results


def _CheckPolicyHistograms(input_api, output_api, policies):
  root = input_api.change.RepositoryRoot()
  histograms = input_api.os_path.join(
      root, 'tools', 'metrics', 'histograms', 'enums.xml')
  with open(histograms) as f:
    tree = minidom.parseString(f.read())
  enums = (tree.getElementsByTagName('histogram-configuration')[0]
               .getElementsByTagName('enums')[0]
               .getElementsByTagName('enum'))
  policy_enum = [e for e in enums
                 if e.getAttribute('name') == 'EnterprisePolicies'][0]
  policy_enum_ids = frozenset(int(e.getAttribute('value'))
                              for e in policy_enum.getElementsByTagName('int'))
  policy_id_to_name = {policy['id']: policy['name'] for policy in policies}
  policy_ids = frozenset(policy_id_to_name.keys())

  missing_ids = policy_ids - policy_enum_ids
  extra_ids = policy_enum_ids - policy_ids

  error_missing = ('Policy \'%s\' (id %d) was added to '
                   'policy_templates.json but not to '
                   'src/tools/metrics/histograms/enums.xml. Please update '
                   'both files. To regenerate the policy part of enums.xml, '
                   'run:\n'
                   'python tools/metrics/histograms/update_policies.py')
  error_extra = ('Policy id %d was found in '
                 'src/tools/metrics/histograms/enums.xml, but no policy with '
                 'this id exists in policy_templates.json. To regenerate the '
                 'policy part of enums.xml, run:\n'
                 'python tools/metrics/histograms/update_policies.py')
  results = []
  for policy_id in missing_ids:
    results.append(
        output_api.PresubmitError(error_missing %
                                  (policy_id_to_name[policy_id], policy_id)))
  for policy_id in extra_ids:
    results.append(output_api.PresubmitError(error_extra % policy_id))
  return results


def _CheckPolicyAtomicGroupsHistograms(input_api, output_api, atomic_groups):
  root = input_api.change.RepositoryRoot()
  histograms = input_api.os_path.join(
      root, 'tools', 'metrics', 'histograms', 'enums.xml')
  with open(histograms) as f:
    tree = minidom.parseString(f.read())
  enums = (tree.getElementsByTagName('histogram-configuration')[0]
               .getElementsByTagName('enums')[0]
               .getElementsByTagName('enum'))
  atomic_group_enum = [e for e in enums
                 if e.getAttribute('name') == 'PolicyAtomicGroups'][0]
  atomic_group_enum_ids = frozenset(int(e.getAttribute('value'))
                              for e in atomic_group_enum
                                .getElementsByTagName('int'))
  atomic_group_id_to_name = {policy['id']: policy['name']
                                    for policy in atomic_groups}
  atomic_group_ids = frozenset(atomic_group_id_to_name.keys())

  missing_ids = atomic_group_ids - atomic_group_enum_ids
  extra_ids = atomic_group_enum_ids - atomic_group_ids

  error_missing = ('Policy atomic group \'%s\' (id %d) was added to '
                   'policy_templates.json but not to '
                   'src/tools/metrics/histograms/enums.xml. Please update '
                   'both files. To regenerate the policy part of enums.xml, '
                   'run:\n'
                   'python tools/metrics/histograms/update_policies.py')
  error_extra = ('Policy atomic group id %d was found in '
                 'src/tools/metrics/histograms/enums.xml, but no policy with '
                 'this id exists in policy_templates.json. To regenerate the '
                 'policy part of enums.xml, run:\n'
                 'python tools/metrics/histograms/update_policies.py')
  results = []
  for atomic_group_id in missing_ids:
    results.append(output_api.PresubmitError(error_missing %
                              (atomic_group_id_to_name[atomic_group_id],
                              atomic_group_id)))
  for atomic_group_id in extra_ids:
    results.append(output_api.PresubmitError(error_extra % atomic_group_id))
  return results

def _CheckMissingPlaceholders(input_api, output_api, template_path):
  with open(template_path) as f:
    template_data = eval(f.read(), {})

  results = []
  items = template_data['policy_definitions'] \
          + [msg for msg in template_data['messages'].values()]
  for item in items:
    for key in ['desc', 'text']:
      if not key in item:
        continue
      node = minidom.parseString('<msg>%s</msg>' % item[key]).childNodes[0]
      for child in node.childNodes:
        if child.nodeType == minidom.Node.TEXT_NODE and '$' in child.data:
          warning = ('Character \'$\' found outside of a placeholder in "%s". '
                     'Should it be in a placeholder ?') % item[key]
          results.append(output_api.PresubmitPromptWarning(warning))
  return results


def _CommonChecks(input_api, output_api):
  results = []
  root = input_api.change.RepositoryRoot()
  template_path = template_path = input_api.os_path.join(
      root, 'components', 'policy', 'resources', 'policy_templates.json')
  # policies in chrome/test/data/policy/policy_test_cases.json.
  test_cases_path = input_api.os_path.join(
      root, 'chrome', 'test', 'data', 'policy', 'policy_test_cases.json')
  affected_files = input_api.change.AffectedFiles()

  results.extend(_CheckMissingPlaceholders(input_api, output_api,
      template_path))
  template_changed = any(f.AbsoluteLocalPath() == template_path \
    for f in affected_files)
  tests_changed = any(f.AbsoluteLocalPath() == test_cases_path \
    for f in affected_files)

  if template_changed or tests_changed:
    try:
      policies = _GetPolicyTemplates(template_path)
    except:
      results.append(output_api.PresubmitError('Invalid Python/JSON syntax.'))
      return results
    results.extend(_CheckPolicyTestCases(input_api, output_api, policies))
    if template_changed:
      results.extend(_CheckPolicyTemplatesSyntax(input_api, output_api))
      results.extend(_CheckPolicyHistograms(input_api, output_api, policies))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import template_writer


class GpoEditorWriter(template_writer.TemplateWriter):
  '''Abstract class for ADM and ADMX writers.

  It includes deprecated policies in its output, and places them in a dedicated
  'DeprecatedPolicies' group. Every deprecated policy has the same description.

  It is a superclass for AdmWriter and AdmxWriter.
  '''

  def IsDeprecatedPolicySupported(self, policy):
    # Include deprecated policies in the output.
    return True

  def IsVersionSupported(self, policy, supported_on):
    # Include deprecated policies in the 'DeprecatedPolicies' group, even if
    # they aren't supported anymore.
    major_version = self._GetChromiumMajorVersion()
    if not major_version:
      return True

    since_version = supported_on.get('since_version', None)

    return not since_version or major_version >= int(since_version)

  def IsPolicyOnWin7Only(self, policy):
    ''' Returns true if the policy is supported on win7 only.'''
    for suppported_on in policy.get('supported_on', []):
      if 'win7' == suppported_on.get('platform', []):
        return True
    return False

  def _IsRemovedPolicy(self, policy):
    major_version = self._GetChromiumMajorVersion()
    for supported_on in policy.get('supported_on', []):
      if '*' in self.platforms or supported_on['platform'] in self.platforms:
        until_version = supported_on.get('until_version', None)
        if not until_version or major_version <= int(until_version):
          # The policy is still supported, return False.
          return False
    # No platform+version combo supports this version, return True.
    return True

  def _FilterPolicies(self, predicate, policy_list):
    filtered_policies = []
    for policy in policy_list:
      if policy['type'] == 'group':
        for p in policy['policies']:
          if predicate(p):
            filtered_policies.append(p)
      else:
        if predicate(policy):
          filtered_policies.append(policy)
    return filtered_policies

  def _RemovePoliciesFromList(self, policy_list, policies_to_remove):
    '''Remove policies_to_remove from groups and the top-level list.'''
    # We only compare the 'name' property.
    policies_to_remove = set([p['name'] for p in policies_to_remove])

    # Remove from top-level list.
    policy_list[:] = [
        p for p in policy_list if p['name'] not in policies_to_remove
    ]

    # Remove from groups.
    for group in policy_list:
      if group['type'] != 'group':
        continue
      group['policies'] = [
          p for p in group['policies'] if p['name'] not in policies_to_remove
      ]

    # Remove empty groups.
    policy_list[:] = [
        p for p in policy_list if p['type'] != 'group' or p['policies']
    ]

  def _MovePolicyGroup(self, policy_list, predicate, policy_desc, group):
    '''Remove policies from |policy_list| that satisfy |predicate| and add them
    to |group|.'''
    filtered_policies = self._FilterPolicies(predicate, policy_list)
    self._RemovePoliciesFromList(policy_list, filtered_policies)

    for p in filtered_policies:
      p['desc'] = policy_desc

    group['policies'] = filtered_policies

  def PreprocessPolicies(self, policy_list):
    '''Put policies under the DeprecatedPolicies/RemovedPolicies groups.'''
    removed_policies_group = {
        'name': 'RemovedPolicies',
        'type': 'group',
        'caption': self.messages['removed_policy_group_caption']['text'],
        'desc': self.messages['removed_policy_group_desc']['text'],
        'policies': []
    }
    self._MovePolicyGroup(
        policy_list,
        lambda p: self._IsRemovedPolicy(p),
        self.messages['removed_policy_desc']['text'],
        removed_policies_group)

    deprecated_policies_group = {
        'name': 'DeprecatedPolicies',
        'type': 'group',
        'caption': self.messages['deprecated_policy_group_caption']['text'],
        'desc': self.messages['deprecated_policy_group_desc']['text'],
        'policies': []
    }
    self._MovePolicyGroup(
        policy_list,
        lambda p: p.get('deprecated', False),
        self.messages['deprecated_policy_desc']['text'],
        deprecated_policies_group)

    policy_list.append(deprecated_policies_group)
    policy_list.append(removed_policies_group)

    return super(GpoEditorWriter, self).SortPoliciesGroupsFirst(policy_list)

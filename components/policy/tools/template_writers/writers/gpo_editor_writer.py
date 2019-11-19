#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
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
    #
    # TODO(crbug.com/463990): Eventually exclude some policies, e.g. if they
    # were deprecated a long time ago.
    if policy.get('deprecated', False):
      return True

    return super(GpoEditorWriter, self).IsVersionSupported(policy, supported_on)

  def IsPolicyOnWin7Only(self, policy):
    ''' Returns true if the policy is supported on win7 only.'''
    for suppported_on in policy.get('supported_on', []):
      if 'win7' in suppported_on.get('platforms', []):
        return True
    return False


  def _FindDeprecatedPolicies(self, policy_list):
    deprecated_policies = []
    for policy in policy_list:
      if policy['type'] == 'group':
        for p in policy['policies']:
          if p.get('deprecated', False):
            deprecated_policies.append(p)
      else:
        if policy.get('deprecated', False):
          deprecated_policies.append(policy)
    return deprecated_policies

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

  def PreprocessPolicies(self, policy_list):
    '''Put deprecated policies under the  'DeprecatedPolicies' group.'''
    deprecated_policies = self._FindDeprecatedPolicies(policy_list)

    self._RemovePoliciesFromList(policy_list, deprecated_policies)

    for p in deprecated_policies:
      # TODO(crbug.com/463990): Also include an alternative policy in the
      # description, if the policy was replaced by a newer one.
      p['desc'] = self.messages['deprecated_policy_desc']['text']

    deprecated_group = {
        'name': 'DeprecatedPolicies',
        'type': 'group',
        'caption': self.messages['deprecated_policy_group_caption']['text'],
        'desc': self.messages['deprecated_policy_group_desc']['text'],
        'policies': deprecated_policies
    }
    policy_list.append(deprecated_group)

    return super(GpoEditorWriter, self).SortPoliciesGroupsFirst(policy_list)

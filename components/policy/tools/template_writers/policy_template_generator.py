#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import re


def IsGroupOrAtomicGroup(policy):
  return policy['type'] == 'group' or policy['type'] == 'atomic_group'


class PolicyTemplateGenerator:
  '''Generates template text for a particular platform.

  This class is used to traverse a JSON structure from a .json template
  definition metafile and merge GUI message string definitions that come
  from a .grd resource tree onto it. After this, it can be used to output
  this data to policy template files using TemplateWriter objects.
  '''

  def _ImportMessage(self, msg_txt):
    msg_txt = msg_txt.decode('utf-8')
    lines = msg_txt.split('\n')

    # Strip any extra leading spaces, but keep useful indentation:
    min_leading_spaces = min(list(self._IterateLeadingSpaces(lines)) or [0])
    if min_leading_spaces > 0:
      lstrip_pattern = re.compile('^[ ]{1,%s}' % min_leading_spaces)
      lines = [lstrip_pattern.sub('', line) for line in lines]
    # Strip all trailing spaces:
    lines = [line.rstrip() for line in lines]
    return "\n".join(lines)

  def _IterateLeadingSpaces(self, lines):
    '''Yields the number of leading spaces on each line, skipping lines which
    have no leading spaces.'''
    for line in lines:
      match = re.search('^[ ]+', line)
      if match:
        yield len(match.group(0))

  def __init__(self, config, policy_data):
    '''Initializes this object with all the data necessary to output a
    policy template.

    Args:
      config: Writer configuration.
      policy_data: The list of defined policies and groups, as parsed from the
        policy metafile. See
          components/policy/resources/policy_templates.json
        for description and content.
            '''
    # List of all the policies. Create a copy since the data is modified.
    self._policy_data = copy.deepcopy(policy_data)
    # Localized messages to be inserted to the policy_definitions structure:
    self._messages = self._policy_data['messages']
    self._config = config
    for key in self._messages.keys():
      self._messages[key]['text'] = self._ImportMessage(
          self._messages[key]['text'])
    self._AddGroups(self._policy_data['policy_definitions'])
    self._AddAtomicGroups(self._policy_data['policy_definitions'],
                          self._policy_data['policy_atomic_group_definitions'])
    self._policy_data[
        'policy_atomic_group_definitions'] = self._ExpandAtomicGroups(
            self._policy_data['policy_definitions'],
            self._policy_data['policy_atomic_group_definitions'])
    self._ProcessPolicyList(
        self._policy_data['policy_atomic_group_definitions'])
    self._policy_data['policy_definitions'] = self._ExpandGroups(
        self._policy_data['policy_definitions'])
    self._policy_definitions = self._policy_data['policy_definitions']
    self._ProcessPolicyList(self._policy_definitions)

  def _ProcessSupportedOn(self, supported_on):
    '''Parses and converts the string items of the list of supported platforms
    into dictionaries.

    Args:
      supported_on: The list of supported platforms. E.g.:
        ['chrome.win:8-10', 'chrome_frame:10-']

    Returns:
      supported_on: The list with its items converted to dictionaries. E.g.:
      [{
        'product': 'chrome',
        'platforms': 'win',
        'since_version': '8',
        'until_version': '10'
      }, {
        'product': 'chrome_frame',
        'platforms': 'win',
        'since_version': '10',
        'until_version': ''
      }]
    '''
    result = []
    for supported_on_item in supported_on:
      product_platform_part, version_part = supported_on_item.split(':')

      if '.' in product_platform_part:
        product, platform = product_platform_part.split('.')
        if platform == '*':
          # e.g.: 'chrome.*:8-10'
          platforms = ['linux', 'mac', 'win']
        else:
          # e.g.: 'chrome.win:-10'
          platforms = [platform]
      else:
        # e.g.: 'chrome_frame:7-'
        product, platform = {
            'android': ('chrome', 'android'),
            'webview_android': ('webview', 'android'),
            'ios': ('chrome', 'ios'),
            'chrome_os': ('chrome_os', 'chrome_os'),
            'chrome_frame': ('chrome_frame', 'win'),
        }[product_platform_part]
        platforms = [platform]
      since_version, until_version = version_part.split('-')
      result.append({
          'product': product,
          'platforms': platforms,
          'since_version': since_version,
          'until_version': until_version
      })
    return result

  def _ProcessPolicy(self, policy):
    '''Processes localized message strings in a policy or a group.
     Also breaks up the content of 'supported_on' attribute into a list.

    Args:
      policy: The data structure of the policy or group, that will get message
        strings here.
    '''
    if policy['type'] != 'atomic_group':
      policy['desc'] = self._ImportMessage(policy['desc'])
    policy['caption'] = self._ImportMessage(policy['caption'])
    if 'label' in policy:
      policy['label'] = self._ImportMessage(policy['label'])
    if 'arc_support' in policy:
      policy['arc_support'] = self._ImportMessage(policy['arc_support'])

    if IsGroupOrAtomicGroup(policy):
      self._ProcessPolicyList(policy['policies'])
    elif policy['type'] in ('string-enum', 'int-enum', 'string-enum-list'):
      # Iterate through all the items of an enum-type policy, and add captions.
      for item in policy['items']:
        item['caption'] = self._ImportMessage(item['caption'])
      if 'supported_on' in item:
        item['supported_on'] = self._ProcessSupportedOn(item['supported_on'])
    if not IsGroupOrAtomicGroup(policy):
      if not 'label' in policy:
        # If 'label' is not specified, then it defaults to 'caption':
        policy['label'] = policy['caption']
      policy['supported_on'] = self._ProcessSupportedOn(policy['supported_on'])

  def _ProcessPolicyList(self, policy_list):
    '''Adds localized message strings to each item in a list of policies and
    groups. Also breaks up the content of 'supported_on' attributes into lists
    of dictionaries.

    Args:
      policy_list: A list of policies and groups. Message strings will be added
        for each item and to their child items, recursively.
    '''
    for policy in policy_list:
      self._ProcessPolicy(policy)

  def GetTemplateText(self, template_writer):
    '''Generates the text of the template from the arguments given
    to the constructor, using a given TemplateWriter.

    Args:
      template_writer: An object implementing TemplateWriter. Its methods
        are called here for each item of self._policy_data.

    Returns:
      The text of the generated template.
    '''
    # Create a copy, so that writers can't screw up subsequent writers.
    policy_data_copy = copy.deepcopy(self._policy_data)
    return template_writer.WriteTemplate(policy_data_copy)


  def _AddGroups(self, policy_list):
    '''Adds a 'group' field, which is set to be the group's name, to the
       policies that are part of a group.

    Args:
      policy_list: A list of policies and groups whose policies will have a
      'group' field added.
    '''
    groups = [policy for policy in policy_list if policy['type'] == 'group']
    policy_lookup = {
        policy['name']: policy
        for policy in policy_list
        if not IsGroupOrAtomicGroup(policy)
    }
    for group in groups:
      for policy_name in group['policies']:
        policy_lookup[policy_name]['group'] = group['name']

  def _AddAtomicGroups(self, policy_list, policy_atomic_groups):
    '''Adds an 'atomic_group' field to the policies that are part of an atomic
    group.

    Args:
      policy_list: A list of policies and groups.
      policy_atomic_groups: A list of policy atomic groups
    '''
    policy_lookup = {
        policy['name']: policy
        for policy in policy_list
        if not IsGroupOrAtomicGroup(policy)
    }
    for group in policy_atomic_groups:
      for policy_name in group['policies']:
        policy_lookup[policy_name]['atomic_group'] = group['name']
        break

  def _ExpandAtomicGroups(self, policy_list, policy_atomic_groups):
    '''Replaces policies names inside atomic group definitions for actual
    policies definitions.

    Args:
      policy_list: A list of policies and groups.

    Returns:
      Modified policy_list
    '''
    policies = [
        policy for policy in policy_list if not IsGroupOrAtomicGroup(policy)
    ]
    for group in policy_atomic_groups:
      group['type'] = 'atomic_group'
    expanded = self._ExpandGroups(policies + policy_atomic_groups)
    expanded = [policy for policy in expanded if IsGroupOrAtomicGroup(policy)]
    return copy.deepcopy(expanded)

  def _ExpandGroups(self, policy_list):
    '''Replaces policies names inside group definitions for actual policies
    definitions. If policy does not belong to any group, leave it as is.

    Args:
      policy_list: A list of policies and groups.

    Returns:
      Modified policy_list
    '''
    groups = [policy for policy in policy_list if IsGroupOrAtomicGroup(policy)]
    policies = {
        policy['name']: policy
        for policy in policy_list
        if not IsGroupOrAtomicGroup(policy)
    }
    policies_in_groups = set()
    result_policies = []
    for group in groups:
      group_policies = group['policies']
      expanded_policies = [
          policies[policy_name] for policy_name in group_policies
      ]
      assert policies_in_groups.isdisjoint(group_policies)
      policies_in_groups.update(group_policies)
      group['policies'] = expanded_policies
      result_policies.append(group)

    result_policies.extend([
        policy for policy in policy_list if not IsGroupOrAtomicGroup(policy) and
        policy['name'] not in policies_in_groups
    ])
    return result_policies

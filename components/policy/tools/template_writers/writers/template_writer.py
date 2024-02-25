#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TemplateWriter(object):
  '''Abstract base class for writing policy templates in various formats.
  The methods of this class will be called by PolicyTemplateGenerator.
  '''
  def __init__(self, platforms, config):
    '''Initializes a TemplateWriter object.

    Args:
      platforms: List of platforms for which this writer can write policies.
      config: A dictionary of information required to generate the template.
        It contains some key-value pairs, including the following examples:
          'build': 'chrome' or 'chromium'
          'branding': 'Google Chrome' or 'Chromium'
          'mac_bundle_id': The Mac bundle id of Chrome. (Only set when building
            for Mac.)
    '''
    self.platforms = platforms
    self.config = config

  def IsDeprecatedPolicySupported(self, policy):
    '''Checks if the given deprecated policy is supported by the writer.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include the deprecated 'policy' in its
      output.
    '''
    return False

  def IsFuturePolicySupported(self, policy):
    '''Checks if the given future policy is supported by the writer.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include the unreleased 'policy' in its
      output.
    '''
    return False

  def IsCloudOnlyPolicySupported(self, policy):
    '''Checks if the given cloud only policy is supported by the writer.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include the cloud only 'policy' in its
      output.
    '''
    return False

  def IsInternalOnlyPolicySupported(self, policy):
    '''Checks if the given internal policy is supported by the writer.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include the internal only 'policy' in its
      output.
    '''
    return False

  def IsPolicySupported(self, policy):
    '''Checks if the given policy is supported by the writer.
    In other words, the set of platforms supported by the writer
    has a common subset with the set of platforms that support
    the policy.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include 'policy' in its output.
    '''
    if ('deprecated' in policy and policy['deprecated'] is True
        and not self.IsDeprecatedPolicySupported(policy)):
      return False

    if (self.IsCloudOnlyPolicy(policy)
        and not self.IsCloudOnlyPolicySupported(policy)):
      return False

    if (self.IsInternalOnlyPolicy(policy)
        and not self.IsInternalOnlyPolicySupported(policy)):
      return False

    for supported_on in policy['supported_on']:
      if not self.IsVersionSupported(policy, supported_on):
        continue
      if '*' in self.platforms or supported_on['platform'] in self.platforms:
        return True

    if self.IsFuturePolicySupported(policy):
      if '*' in self.platforms and policy['future_on']:
        return True
      for future in policy['future_on']:
        if future['platform'] in self.platforms:
          return True
    return False

  def GetPolicyFeature(self, policy, feature_name, value=None):
    '''Returns policy feature with |feature_name| if exsits. Otherwise, returns
    |value|.'''
    return policy.get('features', {}).get(feature_name, value)

  def CanBeRecommended(self, policy):
    '''Checks if the given policy can be recommended.'''
    return self.GetPolicyFeature(policy, 'can_be_recommended', False)

  def CanBeMandatory(self, policy):
    '''Checks if the given policy can be mandatory.'''
    return self.GetPolicyFeature(policy, 'can_be_mandatory', True)

  def IsCloudOnlyPolicy(self, policy):
    '''Checks if the given policy is cloud only'''
    return self.GetPolicyFeature(policy, 'cloud_only', False)

  def IsInternalOnlyPolicy(self, policy):
    '''Checks if the given policy is internal only'''
    return self.GetPolicyFeature(policy, 'internal_only', False)

  def IsPolicyOrItemSupportedOnPlatform(self, item, platform, product=None):
    '''Checks if |item| is supported on |product| for |platform|. If
    |product| is not specified, only the platform support is checked.
    If |management| is specified, also checks for support for Chrome OS
    management type.

    Args:
      item: The dictionary of the policy or item.
      platform: The platform to check; one of
        'win', 'mac', 'linux', 'chrome_os', 'android'.
      product: Optional product to check; one of
        'chrome', 'chrome_frame', 'chrome_os', 'webview'.
    '''
    for supported_on in item['supported_on']:
      if (platform == supported_on['platform']
          and (not product or product in supported_on['product'])
          and self.IsVersionSupported(item, supported_on)):
        return True
    if self.IsFuturePolicySupported(item):
      if (product and {
          'platform': platform,
          'product': product
      } in item.get('future_on', [])):
        return True
      if (not product and filter(lambda f: f['platform'] == platform,
                                 item.get('future_on', []))):
        return True
    return False

  def IsPolicySupportedOnWindows(self, policy, product=None):
    ''' Checks if |policy| is supported on any Windows platform.

    Args:
      policy: The dictionary of the policy.
      product: Optional product to check; one of
        'chrome', 'chrome_frame', 'chrome_os', 'webview'
    '''
    return (self.IsPolicyOrItemSupportedOnPlatform(policy, 'win', product)
            or self.IsPolicyOrItemSupportedOnPlatform(policy, 'win7', product))

  def IsVersionSupported(self, policy, supported_on):
    '''Checks whether the policy is supported on current version'''
    major_version = self._GetChromiumMajorVersion()
    if not major_version:
      return True

    since_version = supported_on.get('since_version', None)
    until_version = supported_on.get('until_version', None)

    return ((not since_version or int(since_version) <= major_version)
            and (not until_version or int(until_version) >= major_version))

  def _GetChromiumVersionString(self):
    '''Returns the Chromium version string stored in the environment variable
    version (if it is set).

    Returns: The Chromium version string or None if it has not been set.'''

    return self.config.get('version', None)

  def _GetChromiumMajorVersion(self):
    ''' Returns the major version of Chromium if it exists
    in config.
    '''
    return self.config.get('major_version', None)

  def _GetPoliciesForWriter(self, group):
    '''Filters the list of policies in the passed group that are supported by
    the writer.

    Args:
      group: The dictionary of the policy group.

    Returns: The list of policies of the policy group that are compatible
      with the writer.
    '''
    if not 'policies' in group:
      return []
    result = []
    for policy in group['policies']:
      if self.IsPolicySupported(policy):
        result.append(policy)
    return result

  def Init(self):
    '''Initializes the writer. If the WriteTemplate method is overridden, then
    this method must be called as first step of each template generation
    process.
    '''
    pass

  def WriteTemplate(self, template):
    '''Writes the given template definition.

    Args:
      template: Template definition to write.

    Returns:
      Generated output for the passed template definition.
    '''
    self.messages = template['messages']
    self.Init()
    template['policy_definitions'] = \
        self.PreprocessPolicies(template['policy_definitions'])
    self.BeginTemplate()
    self.WritePolicies(template['policy_definitions'])
    self.EndTemplate()

    return self.GetTemplateText()

  def PreprocessPolicies(self, policy_list):
    '''Preprocesses a list of policies according to a given writer's needs.
    Preprocessing steps include sorting policies and stripping unneeded
    information such as groups (for writers that ignore them).
    Subclasses are encouraged to override this method, overriding
    implementations may call one of the provided specialized implementations.
    The default behaviour is to use SortPoliciesGroupsFirst().

    Args:
      policy_list: A list containing the policies to sort.

    Returns:
      The sorted policy list.
    '''
    return self.SortPoliciesGroupsFirst(policy_list)

  def WritePolicies(self, policy_list):
    '''Appends the template text corresponding to all the policies into the
    internal buffer.

    Args:
      policy_list: A list containing the policies to write.
    '''
    for policy in policy_list:
      if policy['type'] == 'group':
        child_policies = list(self._GetPoliciesForWriter(policy))
        child_recommended_policies = list(
            filter(self.CanBeRecommended, child_policies))
        # Only write nonempty groups.
        if child_policies:
          # Miscellaneous should not be considered a group.
          treat_as_group = policy['name'] != 'Miscellaneous'
          if treat_as_group:
            self.BeginPolicyGroup(policy)
          for child_policy in child_policies:
            # Nesting of groups is currently not supported.
            self.WritePolicy(child_policy)
          if treat_as_group:
            self.EndPolicyGroup()
        if child_recommended_policies:
          if treat_as_group:
            self.BeginRecommendedPolicyGroup(policy)
          for child_policy in child_recommended_policies:
            self.WriteRecommendedPolicy(child_policy)
          if treat_as_group:
            self.EndRecommendedPolicyGroup()
      elif self.IsPolicySupported(policy):
        self.WritePolicy(policy)
        if self.CanBeRecommended(policy):
          self.WriteRecommendedPolicy(policy)

  def WritePolicy(self, policy):
    '''Appends the template text corresponding to a policy into the
    internal buffer.

    Args:
      policy: The policy as it is found in the JSON file.
    '''
    raise NotImplementedError()

  def WriteComment(self, comment):
    '''Appends the comment to the internal buffer.

      comment: The comment to be added.
    '''
    raise NotImplementedError()

  def WriteRecommendedPolicy(self, policy):
    '''Appends the template text corresponding to a recommended policy into the
    internal buffer.

    Args:
      policy: The recommended policy as it is found in the JSON file.
    '''
    # TODO
    #raise NotImplementedError()
    pass

  def BeginPolicyGroup(self, group):
    '''Appends the template text corresponding to the beginning of a
    policy group into the internal buffer.

    Args:
      group: The policy group as it is found in the JSON file.
    '''
    pass

  def EndPolicyGroup(self):
    '''Appends the template text corresponding to the end of a
    policy group into the internal buffer.
    '''
    pass

  def BeginRecommendedPolicyGroup(self, group):
    '''Appends the template text corresponding to the beginning of a recommended
    policy group into the internal buffer.

    Args:
      group: The recommended policy group as it is found in the JSON file.
    '''
    pass

  def EndRecommendedPolicyGroup(self):
    '''Appends the template text corresponding to the end of a recommended
    policy group into the internal buffer.
    '''
    pass

  def BeginTemplate(self):
    '''Appends the text corresponding to the beginning of the whole
    template into the internal buffer.
    '''
    raise NotImplementedError()

  def EndTemplate(self):
    '''Appends the text corresponding to the end of the whole
    template into the internal buffer.
    '''
    pass

  def GetTemplateText(self):
    '''Gets the content of the internal template buffer.

    Returns:
      The generated template from the the internal buffer as a string.
    '''
    raise NotImplementedError()

  def SortPoliciesGroupsFirst(self, policy_list):
    '''Sorts a list of policies alphabetically. The order is the
    following: first groups alphabetically by caption, then other policies
    alphabetically by name. The order of policies inside groups is unchanged.

    Args:
      policy_list: The list of policies to sort. Sub-lists in groups will not
        be sorted.
    '''
    policy_list.sort(key=self.GetPolicySortingKeyGroupsFirst)
    return policy_list

  def FlattenGroupsAndSortPolicies(self, policy_list, sorting_key=None):
    '''Sorts a list of policies according to |sorting_key|, defaulting
    to alphabetical sorting if no key is given. If |policy_list| contains
    policies with type="group", it is flattened first, i.e. any groups' contents
    are inserted into the list as first-class elements and the groups are then
    removed.
    '''
    new_list = []
    for policy in policy_list:
      if policy['type'] == 'group':
        for grouped_policy in policy['policies']:
          new_list.append(grouped_policy)
      else:
        new_list.append(policy)
    if sorting_key == None:
      sorting_key = self.GetPolicySortingKeyName
    new_list.sort(key=sorting_key)
    return new_list

  def GetPolicySortingKeyName(self, policy):
    return policy['name']

  def GetPolicySortingKeyGroupsFirst(self, policy):
    '''Extracts a sorting key from a policy. These keys can be used for
    list.sort() methods to sort policies.
    See TemplateWriter.SortPolicies for usage.
    '''
    is_group = policy['type'] == 'group'
    if is_group:
      # Groups are sorted by caption.
      str_key = policy['caption']
    else:
      # Regular policies are sorted by name.
      str_key = policy['name']
    # Groups come before regular policies.
    return (not is_group, str_key)

  def GetLocalizedMessage(self, msg_id):
    '''Returns a localized message for this writer.

    Args:
      msg_id: The identifier of the message.

    Returns:
      The localized message.
    '''
    return self.messages['doc_' + msg_id]['text']

  def HasExpandedPolicyDescription(self, policy):
    '''Returns whether the policy has expanded documentation containing the link
    to the documentation with schema and formatting.
    '''
    return (policy['type'] in ('dict', 'external') or 'url_schema' in policy
            or 'validation_schema' in policy or 'description_schema' in policy)

  def GetExpandedPolicyDescription(self, policy):
    '''Returns the expanded description of the policy containing the link to the
    documentation with schema and formatting.
    '''
    schema_description_link_text = self.GetLocalizedMessage(
        'schema_description_link')
    url = None
    if 'url_schema' in policy:
      url = policy['url_schema']
    if (policy['type'] in ('dict', 'external') or 'validation_schema' in policy
        or 'description_schema' in policy):
      url = (
          'https://cloud.google.com/docs/chrome-enterprise/policies/?policy=' +
          policy['name'])
    return schema_description_link_text.replace('$6', url) if url else ''

#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import gpo_editor_writer
import re

NEWLINE = '\r\n'
POLICY_LIST_URL = '''https://cloud.google.com/docs/chrome-enterprise/policies/?policy='''


def GetWriter(config):
  '''Factory method for creating AdmWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return AdmWriter(['win', 'win7'], config)


class IndentedStringBuilder:
  '''Utility class for building text with indented lines.'''

  def __init__(self):
    self.lines = []
    self.indent = ''

  def AddLine(self, string='', indent_diff=0):
    '''Appends a string with indentation and a linebreak to |self.lines|.

    Args:
      string: The string to print.
      indent_diff: the difference of indentation of the printed line,
        compared to the next/previous printed line. Increment occurs
        after printing the line, while decrement occurs before that.
    '''
    indent_diff *= 2
    if indent_diff < 0:
      self.indent = self.indent[(-indent_diff):]
    if string != '':
      self.lines.append(self.indent + string)
    else:
      self.lines.append('')
    if indent_diff > 0:
      self.indent += ''.ljust(indent_diff)

  def AddLines(self, other):
    '''Appends the content of another |IndentedStringBuilder| to |self.lines|.
    Indentation of the added lines will be the sum of |self.indent| and
    their original indentation.

    Args:
      other: The buffer from which lines are copied.
    '''
    for line in other.lines:
      self.AddLine(line)

  def ToString(self):
    '''Returns |self.lines| as text string.'''
    return NEWLINE.join(self.lines)


class AdmWriter(gpo_editor_writer.GpoEditorWriter):
  '''Class for generating policy templates in Windows ADM format.
  It is used by PolicyTemplateGenerator to write ADM files.
  '''

  TYPE_TO_INPUT = {
      'string': 'EDITTEXT',
      'int': 'NUMERIC',
      'string-enum': 'DROPDOWNLIST',
      'int-enum': 'DROPDOWNLIST',
      'list': 'LISTBOX',
      'string-enum-list': 'LISTBOX',
      'dict': 'EDITTEXT',
      'external': 'EDITTEXT'
  }

  def _Escape(self, string):
    return string.replace('.', '_')

  def _AddGuiString(self, name, value):
    # The |name| must be escaped.
    assert name == self._Escape(name)
    # Escape newlines in the value.
    value = value.replace('\n', '\\n')
    if name in self.strings_seen:
      err = ('%s was added as "%s" and now added again as "%s"' %
             (name, self.strings_seen[name], value))
      assert value == self.strings_seen[name], err
    else:
      self.strings_seen[name] = value
      line = '%s="%s"' % (name, value)
      self.strings.AddLine(line)

  def _WriteSupported(self, builder, is_win7_only):
    builder.AddLine('#if version >= 4', 1)
    key = 'win_supported_os_win7' if is_win7_only else 'win_supported_os'
    supported_on_text = self.config[key]
    builder.AddLine('SUPPORTED !!' + supported_on_text)
    builder.AddLine('#endif', -1)

  def _WritePart(self, policy, key_name, builder):
    '''Writes the PART ... END PART section of a policy.

    Args:
      policy: The policy to write to the output.
      key_name: The registry key backing the policy.
      builder: Builder to append lines to.
    '''
    policy_part_name = self._Escape(policy['name'] + '_Part')
    self._AddGuiString(policy_part_name, policy['label'])

    # Print the PART ... END PART section:
    builder.AddLine()
    adm_type = self.TYPE_TO_INPUT[policy['type']]
    builder.AddLine('PART !!%s  %s' % (policy_part_name, adm_type), 1)
    if policy['type'] in ('list', 'string-enum-list'):
      # Note that the following line causes FullArmor ADMX Migrator to create
      # corrupt ADMX files. Please use admx_writer to get ADMX files.
      builder.AddLine('KEYNAME "%s\\%s"' % (key_name, policy['name']))
      builder.AddLine('VALUEPREFIX ""')
    else:
      builder.AddLine('VALUENAME "%s"' % policy['name'])
    if policy['type'] == 'int':
      # The default max for NUMERIC values is 9999 which is too small for us.
      max = 2000000000
      min = 0
      if self.PolicyHasRestrictions(policy):
        schema = policy['schema']
        if 'minimum' in schema:
          min = schema['minimum']
        if 'maximum' in schema:
          max = schema['maximum']
      builder.AddLine('MIN %d MAX %d' % (min, max))
    if policy['type'] in ('string', 'dict', 'external'):
      # The default max for EDITTEXT values is 1023, which is too small for
      # big JSON blobs and other string policies.
      builder.AddLine('MAXLEN 1000000')
    if policy['type'] in ('int-enum', 'string-enum'):
      builder.AddLine('ITEMLIST', 1)
      for item in policy['items']:
        if policy['type'] == 'int-enum':
          value_text = 'NUMERIC ' + str(item['value'])
        else:
          value_text = '"' + item['value'] + '"'
        string_id = self._Escape(policy['name'] + '_' + item['name'] +
                                 '_DropDown')
        builder.AddLine('NAME !!%s VALUE %s' % (string_id, value_text))
        self._AddGuiString(string_id, item['caption'])
      builder.AddLine('END ITEMLIST', -1)
    builder.AddLine('END PART', -1)

  def PolicyHasRestrictions(self, policy):
    if 'schema' in policy:
      return any(keyword in policy['schema'] \
        for keyword in ['minimum', 'maximum'])
    return False

  def _WritePolicy(self, policy, key_name, builder):
    policy_name = self._Escape(policy['name'] + '_Policy')
    self._AddGuiString(policy_name, policy['caption'])
    builder.AddLine('POLICY !!%s' % policy_name, 1)
    self._WriteSupported(builder, self.IsPolicyOnWin7Only(policy))
    policy_explain_name = self._Escape(policy['name'] + '_Explain')
    policy_explain = self._GetPolicyExplanation(policy)
    self._AddGuiString(policy_explain_name, policy_explain)
    builder.AddLine('EXPLAIN !!' + policy_explain_name)

    if policy['type'] == 'main':
      builder.AddLine('VALUENAME "%s"' % policy['name'])
      builder.AddLine('VALUEON NUMERIC 1')
      builder.AddLine('VALUEOFF NUMERIC 0')
    else:
      self._WritePart(policy, key_name, builder)

    builder.AddLine('END POLICY', -1)
    builder.AddLine()

  def _GetPolicyExplanation(self, policy):
    '''Returns the explanation for a given policy.
    Includes a link to the relevant documentation on chromium.org.
    '''
    policy_desc = policy.get('desc')
    reference_url = POLICY_LIST_URL + policy['name']
    reference_link_text = self.GetLocalizedMessage('reference_link')
    reference_link_text = reference_link_text.replace('$6', reference_url)

    if policy_desc is not None:
      policy_desc += '\n\n'
      if (not policy.get('deprecated', False) and
          not self._IsRemovedPolicy(policy)):
        policy_desc += reference_link_text
      return policy_desc
    else:
      return reference_link_text

  def WriteComment(self, comment):
    self.lines.AddLine('; ' + comment)

  def WritePolicy(self, policy):
    if self.CanBeMandatory(policy):
      self._WritePolicy(policy, self.winconfig['reg_mandatory_key_name'],
                        self.policies)

  def WriteRecommendedPolicy(self, policy):
    self._WritePolicy(policy, self.winconfig['reg_recommended_key_name'],
                      self.recommended_policies)

  def BeginPolicyGroup(self, group):
    category_name = self._Escape(group['name'] + '_Category')
    self._AddGuiString(category_name, group['caption'])
    self.policies.AddLine('CATEGORY !!' + category_name, 1)

  def EndPolicyGroup(self):
    self.policies.AddLine('END CATEGORY', -1)
    self.policies.AddLine('')

  def BeginRecommendedPolicyGroup(self, group):
    category_name = self._Escape(group['name'] + '_Category')
    self._AddGuiString(category_name, group['caption'])
    self.recommended_policies.AddLine('CATEGORY !!' + category_name, 1)

  def EndRecommendedPolicyGroup(self):
    self.recommended_policies.AddLine('END CATEGORY', -1)
    self.recommended_policies.AddLine('')

  def _CreateTemplate(self, category_path, key_name, policies):
    '''Creates the whole ADM template except for the [Strings] section, and
    returns it as an |IndentedStringBuilder|.

    Args:
      category_path: List of strings representing the category path.
      key_name: Main registry key backing the policies.
      policies: ADM code for all the policies in an |IndentedStringBuilder|.
    '''
    lines = IndentedStringBuilder()
    for part in category_path:
      lines.AddLine('CATEGORY !!' + part, 1)
    lines.AddLine('KEYNAME "%s"' % key_name)
    lines.AddLine()

    lines.AddLines(policies)

    for part in category_path:
      lines.AddLine('END CATEGORY', -1)
    lines.AddLine()

    return lines

  def BeginTemplate(self):
    if self._GetChromiumVersionString() is not None:
      self.WriteComment(self.config['build'] + ' version: ' + \
          self._GetChromiumVersionString())
    self._AddGuiString(self.config['win_supported_os'],
                       self.messages['win_supported_all']['text'])
    self._AddGuiString(self.config['win_supported_os_win7'],
                       self.messages['win_supported_win7']['text'])
    categories = self.winconfig['mandatory_category_path'] + \
                 self.winconfig['recommended_category_path']
    strings = self.winconfig['category_path_strings'].copy()
    if 'adm_category_path_strings' in self.config:
      strings.update(self.config['adm_category_path_strings'])
    for category in categories:
      if (category in strings):
        # Replace {...} by localized messages.
        string = re.sub(r"\{(\w+)\}", \
                        lambda m: self.messages[m.group(1)]['text'], \
                        strings[category])
        self._AddGuiString(category, string)
    # All the policies will be written into self.policies.
    # The final template text will be assembled into self.lines by
    # self.EndTemplate().

  def EndTemplate(self):
    # Copy policies into self.lines.
    policy_class = self.GetClass().upper()
    for class_name in ['MACHINE', 'USER']:
      if policy_class != 'BOTH' and policy_class != class_name:
        continue
      self.lines.AddLine('CLASS ' + class_name, 1)
      self.lines.AddLines(
          self._CreateTemplate(self.winconfig['mandatory_category_path'],
                               self.winconfig['reg_mandatory_key_name'],
                               self.policies))
      self.lines.AddLines(
          self._CreateTemplate(self.winconfig['recommended_category_path'],
                               self.winconfig['reg_recommended_key_name'],
                               self.recommended_policies))
      self.lines.AddLine('', -1)
    # Copy user strings into self.lines.
    self.lines.AddLine('[Strings]')
    self.lines.AddLines(self.strings)

  def Init(self):
    # String buffer for building the whole ADM file.
    self.lines = IndentedStringBuilder()
    # String buffer for building the strings section of the ADM file.
    self.strings = IndentedStringBuilder()
    # Map of strings seen, to avoid duplicates.
    self.strings_seen = {}
    # String buffer for building the policies of the ADM file.
    self.policies = IndentedStringBuilder()
    # String buffer for building the recommended policies of the ADM file.
    self.recommended_policies = IndentedStringBuilder()
    # Shortcut to platform-specific ADMX/ADM specific configuration.
    assert len(self.platforms) == 2
    self.winconfig = self.config['win_config'][self.platforms[0]]

  def GetTemplateText(self):
    return self.lines.ToString()

  def GetClass(self):
    return 'Both'

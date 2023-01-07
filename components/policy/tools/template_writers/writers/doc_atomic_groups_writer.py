#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import doc_writer


def GetWriter(config):
  '''Factory method for creating DocAtomicGroupsWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return DocAtomicGroupsWriter(['*'], config)


class DocAtomicGroupsWriter(doc_writer.DocWriter):
  '''Class for generating atomic policy group templates in HTML format.
  The intended use of the generated file is to upload it on
  https://www.chromium.org, therefore its format has some limitations:
  - No HTML and body tags.
  - Restricted set of element attributes: for example no 'class'.
  Because of the latter the output is styled using the 'style'
  attributes of HTML elements. This is supported by the dictionary
  self._STYLES[] and the method self._AddStyledElement(), they try
  to mimic the functionality of CSS classes. (But without inheritance.)

  This class is invoked by PolicyTemplateGenerator to create the HTML
  files.
  '''

  def _AddPolicyRow(self, parent, policy):
    '''Adds a row for the policy in the summary table.

    Args:
      parent: The DOM node of the summary table.
      policy: The data structure of the policy.
    '''
    tr = self._AddStyledElement(parent, 'tr', ['tr'])
    indent = 'padding-left: %dpx;' % (7 + self._indent_level * 14)
    if policy['type'] != 'group':
      # Normal policies get two columns with name and caption.
      name_td = self._AddStyledElement(tr, 'td', ['td', 'td.left'],
                                       {'style': indent})
      policy_ref = './'
      if self.config.get('local', False):
        policy_ref = './chrome_policy_list.html'
      self.AddElement(name_td, 'a', {'href': policy_ref + '#' + policy['name']},
                      policy['name'])
      self._AddStyledElement(tr, 'td', ['td', 'td.right'], {},
                             policy['caption'])
    else:
      # Groups get one column with caption.
      name_td = self._AddStyledElement(tr, 'td', ['td', 'td.left'], {
          'style': indent,
          'colspan': '2'
      })
      self.AddElement(name_td, 'a', {'name': policy['name']}, policy['caption'])

  #
  # Implementation of abstract methods of TemplateWriter:
  #

  def WritePolicy(self, policy):
    self._AddPolicyRow(self._summary_tbody, policy)

  def BeginTemplate(self):
    self._BeginTemplate('group_intro', 'banner')

  def WriteTemplate(self, template):
    '''Writes the given template definition.

    Args:
      template: Template definition to write.

    Returns:
      Generated output for the passed template definition.
    '''
    self.messages = template['messages']
    self.Init()

    policies = self.PreprocessPolicies(
        template['policy_atomic_group_definitions'])

    self.BeginTemplate()
    for policy in policies:
      if policy['type'] != 'atomic_group':
        continue
      self.BeginPolicyGroup(policy)
      for child_policy in policy['policies']:
        # Nesting of groups is currently not supported.
        self.WritePolicy(child_policy)
      self.EndPolicyGroup()
    self.EndTemplate()

    return self.GetTemplateText()

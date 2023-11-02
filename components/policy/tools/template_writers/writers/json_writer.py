#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from textwrap import TextWrapper
from writers import template_writer

TEMPLATE_HEADER = """\
// Policy template for Linux.
// Uncomment the policies you wish to activate and change their values to
// something useful for your case. The provided values are for reference only
// and do not provide meaningful defaults!
{"""

HEADER_DELIMETER = """\
  //-------------------------------------------------------------------------"""


def GetWriter(config):
  '''Factory method for creating JsonWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return JsonWriter(['linux'], config)


class JsonWriter(template_writer.TemplateWriter):
  '''Class for generating policy files in JSON format (for Linux). The
  generated files will define all the supported policies with example values
  set for them. This class is used by PolicyTemplateGenerator to write .json
  files.
  '''

  def PreprocessPolicies(self, policy_list):
    return self.FlattenGroupsAndSortPolicies(policy_list)

  def WriteComment(self, comment):
    self._out.append('// ' + comment)

  def WritePolicy(self, policy):
    example_value_str = json.dumps(policy['example_value'], sort_keys=True)

    # Add comma to the end of the previous line.
    if not self._first_written:
      self._out[-2] += ','

    if not self.CanBeMandatory(policy) and self.CanBeRecommended(policy):
      line = '  // Note: this policy is supported only in recommended mode.'
      self._out.append(line)
      line = '  // The JSON file should be placed in %srecommended.' % \
             self.config['linux_policy_path']
      self._out.append(line)

    line = '  // %s' % policy['caption']
    self._out.append(line)
    self._out.append(HEADER_DELIMETER)
    description = policy['desc']
    if self.HasExpandedPolicyDescription(policy):
      description += ' ' + self.GetExpandedPolicyDescription(policy) + '\n'
    description = self._text_wrapper.wrap(description)
    self._out += description
    line = '  //"%s": %s' % (policy['name'], example_value_str)
    self._out.append('')
    self._out.append(line)
    self._out.append('')

    self._first_written = False

  def BeginTemplate(self):
    if self._GetChromiumVersionString() is not None:
      self.WriteComment(self.config['build'] + ''' version: ''' + \
          self._GetChromiumVersionString())
    self._out.append(TEMPLATE_HEADER)

  def EndTemplate(self):
    self._out.append('}')

  def Init(self):
    self._out = []
    # The following boolean member is true until the first policy is written.
    self._first_written = True
    # Create the TextWrapper object once.
    self._text_wrapper = TextWrapper(
        initial_indent='  // ',
        subsequent_indent='  // ',
        break_long_words=False,
        width=80)

  def GetTemplateText(self):
    return '\n'.join(self._out)

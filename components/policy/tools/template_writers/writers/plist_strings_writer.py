#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import plist_helper
from writers import template_writer


def GetWriter(config):
  '''Factory method for creating PListStringsWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return PListStringsWriter(['mac'], config)


class PListStringsWriter(template_writer.TemplateWriter):
  '''Outputs localized string table files for the Mac policy file.
  These files are named Localizable.strings and they are in the
  [lang].lproj subdirectories of the manifest bundle.
  '''

  def WriteComment(self, comment):
    self._out.append('/* ' + comment + ' */')

  def _AddToStringTable(self, item_name, caption, desc):
    '''Add a title and a description of an item to the string table.

    Args:
      item_name: The name of the item that will get the title and the
        description.
      title: The text of the title to add.
      desc: The text of the description to add.
    '''
    caption = caption.replace('"', '\\"')
    caption = caption.replace('\n', '\\n')
    desc = desc.replace('"', '\\"')
    desc = desc.replace('\n', '\\n')
    self._out.append('%s.pfm_title = \"%s\";' % (item_name, caption))
    self._out.append('%s.pfm_description = \"%s\";' % (item_name, desc))

  def PreprocessPolicies(self, policy_list):
    return self.FlattenGroupsAndSortPolicies(policy_list)

  def WritePolicy(self, policy):
    '''Add strings to the stringtable corresponding a given policy.

    Args:
      policy: The policy for which the strings will be added to the
        string table.
    '''
    desc = policy['desc']
    if policy['type'] in ('int-enum', 'string-enum', 'string-enum-list'):
      # Append the captions of enum items to the description string.
      item_descs = []
      for item in policy['items']:
        item_descs.append(str(item['value']) + ' - ' + item['caption'])
      desc = '\n'.join(item_descs) + '\n' + desc
    if self.HasExpandedPolicyDescription(policy):
      desc += '\n' + self.GetExpandedPolicyDescription(policy)

    self._AddToStringTable(policy['name'], policy['label'], desc)

  def BeginTemplate(self):
    app_name = plist_helper.GetPlistFriendlyName(self.config['app_name'])
    if self._GetChromiumVersionString() is not None:
      self.WriteComment(self.config['build'] + ''' version: ''' + \
          self._GetChromiumVersionString())
    self._AddToStringTable(app_name, self.config['app_name'],
                           self.messages['mac_chrome_preferences']['text'])

  def Init(self):
    # A buffer for the lines of the string table being generated.
    self._out = []

  def GetTemplateText(self):
    return '\n'.join(self._out)

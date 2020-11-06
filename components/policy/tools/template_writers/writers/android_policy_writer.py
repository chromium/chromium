#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from writers import xml_formatted_writer
from xml.dom import minidom
from xml.sax import saxutils as xml_escape


def GetWriter(config):
  '''Factory method for creating AndroidPolicyWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return AndroidPolicyWriter(['android'], config)


def _EscapeResource(resource):
  '''Escape the resource for usage in an Android resource XML file.
  This includes standard XML escaping as well as those specific to Android.
  '''
  if type(resource) == int:
    return str(resource)
  return xml_escape.escape(resource, {"'": "\\'", '"': '\\"', '\\': '\\\\'})


class AndroidPolicyWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Outputs localized Android Resource XML files.
  The policy strings are localized and exposed as string resources for
  consumption through Android's App restriction Schema.
  '''

  # DOM root node of the generated XML document.
  _doc = None
  # The resources node contains all resource 'string' and 'string-array'
  # elements.
  _resources = None

  def AddStringResource(self, name, string):
    '''Add a string resource of the given name.
    '''
    string_node = self._doc.createElement('string')
    string_node.setAttribute('name', name)
    string_node.appendChild(self._doc.createTextNode(_EscapeResource(string)))
    self._resources.appendChild(string_node)

  def AddStringArrayResource(self, name, string_items):
    '''Add a string-array resource of the given name and
    elements from string_items.
    '''
    string_array_node = self._doc.createElement('string-array')
    string_array_node.setAttribute('name', name)
    self._resources.appendChild(string_array_node)
    for item in string_items:
      string_node = self._doc.createElement('item')
      string_node.appendChild(self._doc.createTextNode(_EscapeResource(item)))
      string_array_node.appendChild(string_node)

  def PreprocessPolicies(self, policy_list):
    return self.FlattenGroupsAndSortPolicies(policy_list)

  def CanBeRecommended(self, policy):
    return False

  def WritePolicy(self, policy):
    name = policy['name']
    self.AddStringResource(name + 'Title', policy['caption'])

    # Get the policy description.
    description = policy['desc']
    self.AddStringResource(name + 'Desc', description)

    items = policy.get('items')
    if items is not None:
      items = [
          item for item in items
          if ('supported_on' not in item or
              self.IsPolicyOrItemSupportedOnPlatform(item, 'android'))
      ]
      entries = [item['caption'] for item in items]
      values = [item['value'] for item in items]
      self.AddStringArrayResource(name + 'Entries', entries)
      self.AddStringArrayResource(name + 'Values', values)

  def BeginTemplate(self):
    comment_text = 'DO NOT MODIFY THIS FILE DIRECTLY!\n' \
                   'IT IS GENERATED FROM policy_templates.json.'
    if self._GetChromiumVersionString():
      comment_text += '\n' + self.config['build'] + ' version: '\
                      + self._GetChromiumVersionString()
    comment_node = self._doc.createComment(comment_text)
    self._doc.insertBefore(comment_node, self._resources)

  def Init(self):
    impl = minidom.getDOMImplementation()
    self._doc = impl.createDocument(None, 'resources', None)
    self._resources = self._doc.documentElement

  def GetTemplateText(self):
    return self.ToPrettyXml(self._doc)

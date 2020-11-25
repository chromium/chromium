#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from writers import xml_formatted_writer


def GetWriter(config):
  '''Factory method for instanciating the IOSAppConfigWriter. Every Writer needs
  a GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return IOSAppConfigWriter(['ios'], config)  # platforms unused


class IOSAppConfigWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Simple writer that writes app_config.xml files.
  '''

  def _WritePolicyPresentation(self, policy, field_group):
    element_type = self.policy_type_to_input_type[policy['type']]
    if element_type:
      attributes = {'type': element_type, 'keyName': policy['name']}
      field = self.AddElement(field_group, 'field', attributes)
      self._AddLocalizedElement(field, 'label', policy['caption'])
      self._AddLocalizedElement(field, 'description', policy['desc'])

  def _AddLocalizedElement(self,
                           parent,
                           element_type,
                           text,
                           localization={'value': 'en-US'}):
    item = self.AddElement(parent, element_type, {})
    localized = self.AddElement(item, 'language', localization)
    self.AddText(localized, text)

  def _WritePresentation(self, policy_list):
    groups = [policy for policy in policy_list if policy['type'] == 'group']
    policies_without_group = [
        policy for policy in policy_list if policy['type'] != 'group'
    ]
    for policy in groups:
      child_policies = self._GetPoliciesForWriter(policy)
      if child_policies:
        field_group = self.AddElement(self._presentation, 'fieldGroup', {})
        self._AddLocalizedElement(field_group, 'name', policy['caption'])
        for child_policy in child_policies:
          self._WritePolicyPresentation(child_policy, field_group)
    for policy in self._GetPoliciesForWriter(
        {'policies': policies_without_group}):
      self._WritePolicyPresentation(policy, self._presentation)

  def IsFuturePolicySupported(self, policy):
    # For now, include all future policies in appconfig.xml.
    return True

  def CreateDocument(self):
    dom_impl = minidom.getDOMImplementation('')
    return dom_impl.createDocument('http://www.w3.org/2001/XMLSchema-instance',
                                   'managedAppConfiguration', None)

  def WriteTemplate(self, template):
    self.messages = template['messages']
    self.Init()
    template['policy_definitions'] = \
        self.PreprocessPolicies(template['policy_definitions'])
    self.BeginTemplate()
    self.WritePolicies(template['policy_definitions'])
    self._WritePresentation(template['policy_definitions'])
    self.EndTemplate()

    return self.GetTemplateText()

  def BeginTemplate(self):
    self._app_config.attributes[
        'xmlns:xsi'] = 'http://www.w3.org/2001/XMLSchema-instance'
    schema_location = '/%s/appconfig/appconfig.xsd' % (self.config['bundle_id'])
    self._app_config.attributes[
        'xsi:noNamespaceSchemaLocation'] = schema_location

    version = self.AddElement(self._app_config, 'version', {})
    self.AddText(version, self.config['version'])

    bundle_id = self.AddElement(self._app_config, 'bundleId', {})
    self.AddText(bundle_id, self.config['bundle_id'])
    self._policies = self.AddElement(self._app_config, 'dict', {})
    self._presentation = self.AddElement(self._app_config, 'presentation',
                                         {'defaultLocale': 'en-US'})

  def WritePolicy(self, policy):
    element_type = self.policy_type_to_xml_tag[policy['type']]
    if element_type:
      attributes = {'keyName': policy['name']}
      # Add a "future=true" attribute for future policies.
      if 'future_on' in policy:
        for config in policy['future_on']:
          if config['platform'] == 'ios':
            attributes['future'] = 'true'
      self.AddElement(self._policies, element_type, attributes)

  def Init(self):
    self._doc = self.CreateDocument()
    self._app_config = self._doc.documentElement
    self.policy_type_to_xml_tag = {
        'string': 'string',
        'int': 'integer',
        'int-enum': 'integer',
        'string-enum': 'string',
        'string-enum-list': 'stringArray',
        'main': 'boolean',
        'list': 'stringArray',
        'dict': 'string',
    }
    self.policy_type_to_input_type = {
        'string': 'input',
        'int': 'input',
        'int-enum': 'select',
        'string-enum': 'select',
        'string-enum-list': 'multiselect',
        'main': 'checkbox',
        'list': 'list',
        'dict': 'input'
    }

  def GetTemplateText(self):
    return self.ToPrettyXml(self._doc)

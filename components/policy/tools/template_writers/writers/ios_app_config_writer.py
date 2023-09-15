#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
import json
from writers import xml_formatted_writer

_POLICY_TYPE_TO_XML_TAG = {
    'string': 'string',
    'int': 'integer',
    'int-enum': 'integer',
    'string-enum': 'string',
    'string-enum-list': 'stringArray',
    'main': 'boolean',
    'list': 'stringArray',
    'dict': 'string',
}

_POLICY_TYPE_TO_INPUT_TYPE = {
    'string': 'input',
    'int': 'input',
    'int-enum': 'select',
    'string-enum': 'select',
    'string-enum-list': 'multiselect',
    'main': 'checkbox',
    'list': 'list',
    'dict': 'input'
}

_JSON_SCHEMA_TYPES = [
    "string", "number", "integer", "boolean", "null", "object", "array"
]


class Error(Exception):
  pass


def _ParseSchemaTypeValueToString(value, type):
  '''Parses the value of a given JSON schema type to a string.
  '''
  if type not in _JSON_SCHEMA_TYPES:
    raise Error('schema type "{}" not supported'.format(type))

  if type == 'integer':
    return '{0:d}'.format(value)

  # Use the default string parser.
  return str(value)


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
    element_type = _POLICY_TYPE_TO_INPUT_TYPE[policy['type']]
    if element_type:
      attributes = {'type': element_type, 'keyName': policy['name']}
      field = self.AddElement(field_group, 'field', attributes)
      self._AddLocalizedElement(field, 'label', policy['caption'])
      self._AddLocalizedElement(field, 'description', policy['desc'])

      if 'enum' in policy['type']:
        options = self.AddElement(field, 'options', {})
        for item in policy['items']:
          self._AddLocalizedElement(
              options, 'option', str(item['caption']), {
                  'value':
                  _ParseSchemaTypeValueToString(item['value'],
                                                policy['schema']['type'])
              })

  def _AddLocalizedElement(self,
                           parent,
                           element_type,
                           text,
                           attributes={},
                           localization={'value': 'en-US'}):
    item = self.AddElement(parent, element_type, attributes)
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

  def _WritePolicyDefaultValue(self, parent, policy):
    if 'default' in policy:
      default_value = self.AddElement(parent, 'defaultValue', {})
      value = self.AddElement(default_value, 'value', {})
      if policy['type'] == 'main':
        if policy['default'] == True:
          self.AddText(value, 'true')
        elif policy['default'] == False:
          self.AddText(value, 'false')
      elif policy['type'] in ['list', 'string-enum-list']:
        for v in policy['default']:
          if value == None:
            value = self.AddElement(default_value, 'value', {})
          self.AddText(value, v)
        value = None
      else:
        self.AddText(value, str(policy['default']))

  def _WritePolicyConstraint(self, parent, policy):
    attrs = {'nullable': 'true'}
    if 'schema' in policy:
      if 'minimum' in policy['schema']:
        attrs['min'] = _ParseSchemaTypeValueToString(
            policy['schema']['minimum'], policy['schema']['type'])
      if 'maximum' in policy['schema']:
        attrs['max'] = _ParseSchemaTypeValueToString(
            policy['schema']['maximum'], policy['schema']['type'])

    constraint = self.AddElement(parent, 'constraint', attrs)
    if 'enum' in policy['type']:
      values_element = self.AddElement(constraint, 'values', {})
      enum = policy['schema']['enum'] if 'enum' in policy['schema'] else policy[
          'schema']['items']['enum']
      for v in enum:
        value = self.AddElement(values_element, 'value', {})
        self.AddText(value,
                     _ParseSchemaTypeValueToString(v, policy['schema']['type']))

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
    schema_location = 'https://storage.googleapis.com/appconfig-media/appconfigschema.xsd'
    self._app_config.attributes[
        'xsi:noNamespaceSchemaLocation'] = schema_location

    version = self.AddElement(self._app_config, 'version', {})
    milestone = self.config['version'].split(".", 1)[0]
    self.AddText(version, milestone)

    bundle_id = self.AddElement(self._app_config, 'bundleId', {})
    self.AddText(bundle_id, self.config['bundle_id'])
    self._policies = self.AddElement(self._app_config, 'dict', {})
    self._presentation = self.AddElement(self._app_config, 'presentation',
                                         {'defaultLocale': 'en-US'})

  def WritePolicy(self, policy):
    element_type = _POLICY_TYPE_TO_XML_TAG[policy['type']]
    if element_type:
      attributes = {'keyName': policy['name']}
      # Add a "<!--FUTURE POLICY-->" comment before future policies.
      if 'future_on' in policy:
        for config in policy['future_on']:
          if config['platform'] == 'ios':
            self.AddComment(self._policies, 'FUTURE POLICY')
      policy_element = self.AddElement(self._policies, element_type, attributes)
      self._WritePolicyDefaultValue(policy_element, policy)
      self._WritePolicyConstraint(policy_element, policy)

  def Init(self):
    self._doc = self.CreateDocument()
    self._app_config = self._doc.documentElement

  def GetTemplateText(self):
    return self.ToPrettyXml(self._doc)

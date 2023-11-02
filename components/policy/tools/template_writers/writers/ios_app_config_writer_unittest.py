#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import json
import unittest

from writers import writer_unittest_common


class IOSAppConfigWriterUnitTests(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for IOSAppConfigWriter.'''

  def _GetTestPolicyTemplate(self, policy_definitions):
    return '''
{
  'policy_definitions': %s,
  'policy_atomic_group_definitions': [],
  'placeholders': [],
  'messages': {},
}
''' % (policy_definitions)

  def _GetExpectedOutput(self, version, policy_definition, policy_presentation):
    if policy_definition:
      definition = '<dict>\n    %s\n  </dict>' % policy_definition
    else:
      definition = '<dict/>'
    if policy_presentation:
      presentation = '<presentation defaultLocale="en-US">\n    %s\n  </presentation>' % policy_presentation
    else:
      presentation = '<presentation defaultLocale="en-US"/>'

    return '''<?xml version="1.0" ?>
<managedAppConfiguration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="https://storage.googleapis.com/appconfig-media/appconfigschema.xsd">
  <version>%s</version>
  <bundleId>com.google.chrome.ios</bundleId>
  %s
  %s
</managedAppConfiguration>''' % (version, definition, presentation)

  def testStringPolicy(self):
    policy_definition = json.dumps([{
        'name': 'string policy',
        'type': 'string',
        'supported_on': ['ios:80-'],
        'caption': 'string caption',
        'desc': 'string description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<string keyName="string policy">
      <constraint nullable="true"/>
    </string>'''
    expected_presentation = '''<field keyName="string policy" type="input">
      <label>
        <language value="en-US">string caption</language>
      </label>
      <description>
        <language value="en-US">string description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testIntPolicy(self):
    policy_definition = json.dumps([{
        'name': 'IntPolicy',
        'type': 'int',
        'supported_on': ['ios:80-'],
        'caption': 'int caption',
        'desc': 'int description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<integer keyName="IntPolicy">
      <constraint nullable="true"/>
    </integer>'''
    expected_presentation = '''<field keyName="IntPolicy" type="input">
      <label>
        <language value="en-US">int caption</language>
      </label>
      <description>
        <language value="en-US">int description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testIntEnumPolicy(self):
    policy_definition = json.dumps([{
        'name':
        'IntEnumPolicy',
        'type':
        'int-enum',
        'supported_on': ['ios:80-'],
        'caption':
        'int-enum caption',
        'desc':
        'int-enum description',
        'schema': {
          'type': 'integer',
          'enum': [0, 1],
        },
        'items': [{
            'name': 'item0',
            'value': 0,
            'caption': 'item 0',
        }, {
            'name': 'item1',
            'value': 1,
            'caption': 'item 1',
        }]
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<integer keyName="IntEnumPolicy">
      <constraint nullable="true">
        <values>
          <value>0</value>
          <value>1</value>
        </values>
      </constraint>
    </integer>'''
    expected_presentation = '''<field keyName="IntEnumPolicy" type="select">
      <label>
        <language value="en-US">int-enum caption</language>
      </label>
      <description>
        <language value="en-US">int-enum description</language>
      </description>
      <options>
        <option value="0">
          <language value="en-US">item 0</language>
        </option>
        <option value="1">
          <language value="en-US">item 1</language>
        </option>
      </options>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumPolicy(self):
    policy_definition = json.dumps([{
        'name':
        'StringEnumPolicy',
        'type':
        'string-enum',
        'supported_on': ['ios:80-'],
        'caption':
        'string-enum caption',
        'desc':
        'string-enum description',
        'schema': {
          'type': 'string',
          'enum': ['0', '1'],
        },
        'items': [{
            'name': 'item0',
            'value': '0',
            'caption': 'item 0',
        }, {
            'name': 'item1',
            'value': '1',
            'caption': 'item 1',
        }]
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<string keyName="StringEnumPolicy">
      <constraint nullable="true">
        <values>
          <value>0</value>
          <value>1</value>
        </values>
      </constraint>
    </string>'''
    expected_presentation = '''<field keyName="StringEnumPolicy" type="select">
      <label>
        <language value="en-US">string-enum caption</language>
      </label>
      <description>
        <language value="en-US">string-enum description</language>
      </description>
      <options>
        <option value="0">
          <language value="en-US">item 0</language>
        </option>
        <option value="1">
          <language value="en-US">item 1</language>
        </option>
      </options>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumListPolicy(self):
    policy_definition = json.dumps([{
        'name':
        'StringEnumListPolicy',
        'type':
        'string-enum-list',
        'supported_on': ['ios:80-'],
        'caption':
        'string-enum-list caption',
        'desc':
        'string-enum-list description',
        'schema': {
            'type': 'array',
            'items': {
                'type': 'string',
                'enum': ['0', '1'],
            },
        },
        'items': [{
            'name': 'item0',
            'value': '0',
            'caption': 'item 0',
        }, {
            'name': 'item1',
            'value': '1',
            'caption': 'item 1',
        }]
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<stringArray keyName="StringEnumListPolicy">
      <constraint nullable="true">
        <values>
          <value>0</value>
          <value>1</value>
        </values>
      </constraint>
    </stringArray>'''
    expected_presentation = '''<field keyName="StringEnumListPolicy" type="multiselect">
      <label>
        <language value="en-US">string-enum-list caption</language>
      </label>
      <description>
        <language value="en-US">string-enum-list description</language>
      </description>
      <options>
        <option value="0">
          <language value="en-US">item 0</language>
        </option>
        <option value="1">
          <language value="en-US">item 1</language>
        </option>
      </options>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testBooleanPolicy(self):
    policy_definition = json.dumps([{
        'name': 'BooleanPolicy',
        'type': 'main',
        'supported_on': ['ios:80-'],
        'caption': 'boolean caption',
        'desc': 'boolean description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<boolean keyName="BooleanPolicy">
      <constraint nullable="true"/>
    </boolean>'''
    expected_presentation = '''<field keyName="BooleanPolicy" type="checkbox">
      <label>
        <language value="en-US">boolean caption</language>
      </label>
      <description>
        <language value="en-US">boolean description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testListPolicy(self):
    policy_definition = json.dumps([{
        'name': 'ListPolicy',
        'type': 'list',
        'supported_on': ['ios:80-'],
        'caption': 'list caption',
        'desc': 'list description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<stringArray keyName="ListPolicy">
      <constraint nullable="true"/>
    </stringArray>'''
    expected_presentation = '''<field keyName="ListPolicy" type="list">
      <label>
        <language value="en-US">list caption</language>
      </label>
      <description>
        <language value="en-US">list description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testDictPolicy(self):
    policy_definition = json.dumps([{
        'name': 'DictPolicy',
        'type': 'dict',
        'supported_on': ['ios:80-'],
        'caption': 'dict caption',
        'desc': 'dict description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    # Dict policies are not supported by the appconfig.xml format, therefore
    # they are treated as JSON strings.
    expected_configuration = '''<string keyName="DictPolicy">
      <constraint nullable="true"/>
    </string>'''
    expected_presentation = '''<field keyName="DictPolicy" type="input">
      <label>
        <language value="en-US">dict caption</language>
      </label>
      <description>
        <language value="en-US">dict description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testFuturePolicy(self):
    policy_definition = json.dumps([{
        'name': 'FuturePolicy',
        'type': 'string',
        'future_on': ['ios'],
        'caption': 'string caption',
        'desc': 'string description'
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<!--FUTURE POLICY-->
    <string keyName="FuturePolicy">
      <constraint nullable="true"/>
    </string>'''
    expected_presentation = '''<field keyName="FuturePolicy" type="input">
      <label>
        <language value="en-US">string caption</language>
      </label>
      <description>
        <language value="en-US">string description</language>
      </description>
    </field>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())

  def testPolicyWithGroup(self):
    policy_definition = json.dumps([{
        'name': 'PolicyInGroup',
        'type': 'string',
        'supported_on': ['ios:80-'],
        'caption': 'string caption',
        'desc': 'string description'
    }, {
        'name': 'DummyGroup',
        'type': 'group',
        'caption': 'Dummy Group',
        'desc': 'Dummy group for testing',
        'policies': ['PolicyInGroup']
    }])
    policy_json = self._GetTestPolicyTemplate(policy_definition)
    expected_configuration = '''<string keyName="PolicyInGroup">
      <constraint nullable="true"/>
    </string>'''
    expected_presentation = '''<fieldGroup>
      <name>
        <language value="en-US">Dummy Group</language>
      </name>
      <field keyName="PolicyInGroup" type="input">
        <label>
          <language value="en-US">string caption</language>
        </label>
        <description>
          <language value="en-US">string description</language>
        </description>
      </field>
    </fieldGroup>'''
    expected = self._GetExpectedOutput('83', expected_configuration,
                                       expected_presentation)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0'
    }, 'ios_app_config')
    self.assertEquals(output.strip(), expected.strip())


if __name__ == '__main__':
  unittest.main()

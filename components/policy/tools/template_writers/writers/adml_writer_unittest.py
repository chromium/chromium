#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for writers.adml_writer."""

import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

from writers import adml_writer
from writers import xml_writer_base_unittest


class AdmlWriterUnittest(xml_writer_base_unittest.XmlWriterBaseTest):

  def setUp(self):
    config = {
        'app_name': 'test',
        'build': 'test',
        'win_supported_os': 'SUPPORTED_TESTOS',
        'win_supported_os_win7': 'SUPPORTED_TESTOS_2',
        'win_config': {
            'win': {
                'mandatory_category_path': ['test_category'],
                'recommended_category_path': ['test_category_recommended'],
                'category_path_strings': {
                    'test_category': 'TestCategory',
                    'test_category_recommended': 'TestCategory - recommended',
                },
            },
            'chrome_os': {
                'mandatory_category_path': ['cros_test_category'],
                'recommended_category_path': ['cros_test_category_recommended'],
                'category_path_strings': {
                    'cros_test_category':
                        'CrOSTestCategory',
                    'cros_test_category_recommended':
                        'CrOSTestCategory - recommended',
                },
            },
        },
    }
    self.writer = self._GetWriter(config)
    self.writer.messages = {
        'win_supported_all': {
            'text': 'Supported on Test OS or higher',
            'desc': 'blah'
        },
        'win_supported_win7': {
            'text': 'Supported on Test OS',
            'desc': 'blah'
        },
        'doc_recommended': {
            'text': 'Recommended',
            'desc': 'bleh'
        },
        'doc_example_value': {
            'text': 'Example value:',
            'desc': 'bluh'
        },
        'doc_legacy_single_line_label': {
            'text': '$6 (deprecated)',
        },
        'doc_schema_description_link': {
            'text': '''See $6'''
        },
    }
    self.writer.Init()

  def _GetWriter(self, config):
    return adml_writer.GetWriter(config)

  def GetCategory(self):
    return "test_category"

  def GetCategoryString(self):
    return "TestCategory"

  def _InitWriterForAddingPolicyGroups(self, writer):
    '''Initialize the writer for adding policy groups. This method must be
    called before the method "BeginPolicyGroup" can be called. It initializes
    attributes of the writer.
    '''
    writer.BeginTemplate()

  def _InitWriterForAddingPolicies(self, writer, policy):
    '''Initialize the writer for adding policies. This method must be
    called before the method "WritePolicy" can be called. It initializes
    attributes of the writer.
    '''
    self._InitWriterForAddingPolicyGroups(writer)
    policy_group = {
        'name': 'PolicyGroup',
        'caption': 'Test Caption',
        'desc': 'This is the test description of the test policy group.',
        'policies': policy,
    }
    writer.BeginPolicyGroup(policy_group)

    string_elements = \
        self.writer._string_table_elem.getElementsByTagName('string')
    for elem in string_elements:
      self.writer._string_table_elem.removeChild(elem)

  def testEmpty(self):
    self.writer.BeginTemplate()
    self.writer.EndTemplate()
    output = self.writer.GetTemplateText()
    expected_output = (
        '<?xml version="1.0" ?><policyDefinitionResources'
        ' revision="1.0" schemaVersion="1.0"><displayName/><description/>'
        '<resources><stringTable><string id="SUPPORTED_TESTOS">Supported on'
        ' Test OS or higher</string>'
        '<string id="SUPPORTED_TESTOS_2">Supported on Test OS</string>'
        '<string id="' + self.GetCategory() + '">' + \
          self.GetCategoryString() + '</string>'
        '<string id="' + self.GetCategory() + '_recommended">' + \
          self.GetCategoryString() + ' - recommended</string>'
        '</stringTable><presentationTable/>'
        '</resources></policyDefinitionResources>')
    self.AssertXMLEquals(output, expected_output)

  def testVersionAnnotation(self):
    self.writer.config['version'] = '39.0.0.0'
    self.writer.BeginTemplate()
    self.writer.EndTemplate()
    output = self.writer.GetTemplateText()
    expected_output = (
        '<?xml version="1.0" ?><policyDefinitionResources'
        ' revision="1.0" schemaVersion="1.0"><!--test version: 39.0.0.0-->'
        '<displayName/><description/><resources><stringTable>'
        '<string id="SUPPORTED_TESTOS">Supported on'
        ' Test OS or higher</string>'
        '<string id="SUPPORTED_TESTOS_2">Supported on Test OS</string>'
        '<string id="' + self.GetCategory() + '">' + \
          self.GetCategoryString() + '</string>'
        '<string id="' + self.GetCategory() + '_recommended">' + \
          self.GetCategoryString() + ' - recommended</string>'
        '</stringTable><presentationTable/>'
        '</resources></policyDefinitionResources>')
    self.AssertXMLEquals(output, expected_output)

  def testPolicyGroup(self):
    empty_policy_group = {
        'name':
            'PolicyGroup',
        'caption':
            'Test Group Caption',
        'desc':
            'This is the test description of the test policy group.',
        'policies': [
            {
                'name': 'PolicyStub2',
                'type': 'main'
            },
            {
                'name': 'PolicyStub1',
                'type': 'main'
            },
        ],
    }
    self._InitWriterForAddingPolicyGroups(self.writer)
    self.writer.BeginPolicyGroup(empty_policy_group)
    self.writer.EndPolicyGroup()
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="SUPPORTED_TESTOS">'
        'Supported on Test OS or higher</string>\n' + \
        '<string id="SUPPORTED_TESTOS_2">Supported on Test OS</string>\n' + \
        '<string id="' + self.GetCategory() + '">' + \
          self.GetCategoryString() + '</string>\n'
        '<string id="' + self.GetCategory() + '_recommended">' + \
          self.GetCategoryString() + ' - recommended</string>\n'
        '<string id="PolicyGroup_group">Test Group Caption</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ''
    self.AssertXMLEquals(output, expected_output)

  def testMainPolicy(self):
    main_policy = {
        'name': 'DummyMainPolicy',
        'type': 'main',
        'caption': 'Main policy caption',
        'desc': 'Main policy test description.'
    }
    self._InitWriterForAddingPolicies(self.writer, main_policy)
    self.writer.WritePolicy(main_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="DummyMainPolicy">Main policy caption</string>\n'
        '<string id="DummyMainPolicy_Explain">'
        'Main policy test description.</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = '<presentation id="DummyMainPolicy"/>'
    self.AssertXMLEquals(output, expected_output)

  def testStringPolicy(self):
    string_policy = {
        'name': 'StringPolicyStub',
        'type': 'string',
        'caption': 'String policy caption',
        'label': 'String policy label',
        'desc': 'This is a test description.',
        'supported_on': [{'platform': 'win'}, {'platform': 'chrome_os'}],
        'example_value': '01:23:45:67:89:ab',
    }
    self._InitWriterForAddingPolicies(self.writer, string_policy)
    self.writer.WritePolicy(string_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="StringPolicyStub">String policy caption</string>\n'
        '<string id="StringPolicyStub_Explain">'
        'This is a test description.\n\n'
        'Example value: 01:23:45:67:89:ab</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="StringPolicyStub">\n'
                       '  <textBox refId="StringPolicyStub">\n'
                       '    <label>String policy label</label>\n'
                       '  </textBox>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testIntPolicy(self):
    int_policy = {
        'name': 'IntPolicyStub',
        'type': 'int',
        'caption': 'Int policy caption',
        'label': 'Int policy label',
        'desc': 'This is a test description.',
    }
    self._InitWriterForAddingPolicies(self.writer, int_policy)
    self.writer.WritePolicy(int_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="IntPolicyStub">Int policy caption</string>\n'
        '<string id="IntPolicyStub_Explain">'
        'This is a test description.</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="IntPolicyStub">\n'
                       '  <decimalTextBox refId="IntPolicyStub">'
                       'Int policy label:</decimalTextBox>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testIntEnumPolicy(self):
    enum_policy = {
        'name':
            'EnumPolicyStub',
        'type':
            'int-enum',
        'caption':
            'Enum policy caption',
        'label':
            'Enum policy label',
        'desc':
            'This is a test description.',
        'items': [
            {
                'name': 'item 1',
                'value': 1,
                'caption': 'Caption Item 1',
            },
            {
                'name': 'item 2',
                'value': 2,
                'caption': 'Caption Item 2',
            },
        ],
    }
    self._InitWriterForAddingPolicies(self.writer, enum_policy)
    self.writer.WritePolicy(enum_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="EnumPolicyStub">Enum policy caption</string>\n'
        '<string id="EnumPolicyStub_Explain">'
        'This is a test description.</string>\n'
        '<string id="EnumPolicyStub_item 1">Caption Item 1</string>\n'
        '<string id="EnumPolicyStub_item 2">Caption Item 2</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="EnumPolicyStub">\n'
                       '  <dropdownList refId="EnumPolicyStub">'
                       'Enum policy label</dropdownList>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testStringEnumPolicy(self):
    enum_policy = {
        'name':
            'EnumPolicyStub',
        'type':
            'string-enum',
        'caption':
            'Enum policy caption',
        'label':
            'Enum policy label',
        'desc':
            'This is a test description.',
        'items': [
            {
                'name': 'item 1',
                'value': 'value 1',
                'caption': 'Caption Item 1',
            },
            {
                'name': 'item 2',
                'value': 'value 2',
                'caption': 'Caption Item 2',
            },
        ],
    }
    self._InitWriterForAddingPolicies(self.writer, enum_policy)
    self.writer.WritePolicy(enum_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="EnumPolicyStub">Enum policy caption</string>\n'
        '<string id="EnumPolicyStub_Explain">'
        'This is a test description.</string>\n'
        '<string id="EnumPolicyStub_item 1">Caption Item 1</string>\n'
        '<string id="EnumPolicyStub_item 2">Caption Item 2</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="EnumPolicyStub">\n'
                       '  <dropdownList refId="EnumPolicyStub">'
                       'Enum policy label</dropdownList>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testListPolicy(self):
    list_policy = {
        'name': 'ListPolicyStub',
        'type': 'list',
        'caption': 'List policy caption',
        'label': 'List policy label',
        'desc': 'This is a test description.',
    }
    self._InitWriterForAddingPolicies(self.writer, list_policy)
    self.writer.WritePolicy(list_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="ListPolicyStub">List policy caption</string>\n'
        '<string id="ListPolicyStub_Explain">'
        'This is a test description.</string>\n'
        '<string id="ListPolicyStubDesc">List policy caption</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="ListPolicyStub">\n'
        '  <listBox refId="ListPolicyStubDesc">List policy label</listBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testStringEnumListPolicy(self):
    list_policy = {
        'name':
            'ListPolicyStub',
        'type':
            'string-enum-list',
        'caption':
            'List policy caption',
        'label':
            'List policy label',
        'desc':
            'This is a test description.',
        'items': [
            {
                'name': 'item 1',
                'value': 'value 1',
                'caption': 'Caption Item 1',
            },
            {
                'name': 'item 2',
                'value': 'value 2',
                'caption': 'Caption Item 2',
            },
        ],
    }
    self._InitWriterForAddingPolicies(self.writer, list_policy)
    self.writer.WritePolicy(list_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="ListPolicyStub">List policy caption</string>\n'
        '<string id="ListPolicyStub_Explain">'
        'This is a test description.</string>\n'
        '<string id="ListPolicyStubDesc">List policy caption</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="ListPolicyStub">\n'
        '  <listBox refId="ListPolicyStubDesc">List policy label</listBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testDictionaryPolicy(self, is_external=False):
    dict_policy = {
        'name': 'DictionaryPolicyStub',
        'type': 'external' if is_external else 'dict',
        'caption': 'Dictionary policy caption',
        'label': 'Dictionary policy label',
        'desc': 'This is a test description.',
    }
    self._InitWriterForAddingPolicies(self.writer, dict_policy)
    self.writer.WritePolicy(dict_policy)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="DictionaryPolicyStub">Dictionary policy caption</string>\n'
        '<string id="DictionaryPolicyStub_Explain">'
        'This is a test description.\n'
        'See https://cloud.google.com/docs/chrome-enterprise/policies/?policy='
        'DictionaryPolicyStub\n</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="DictionaryPolicyStub">\n'
                       '  <textBox refId="DictionaryPolicyStub">\n'
                       '    <label>Dictionary policy label</label>\n'
                       '  </textBox>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)

  def testExternalPolicy(self):
    self.testDictionaryPolicy(is_external=True)

  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Windows.
    self.assertTrue(
        self.writer.IsPolicySupported({
            'supported_on': [{
                'platform': 'win'
            }, {
                'platform': 'aaa'
            }]
        }))
    self.assertFalse(
        self.writer.IsPolicySupported({
            'supported_on': [{
                'platform': 'mac',
            }, {
                'platform': 'aaa'
            }]
        }))

  def testStringEncodings(self):
    enum_policy_a = {
        'name': 'EnumPolicy.A',
        'type': 'string-enum',
        'caption': 'Enum policy A caption',
        'label': 'Enum policy A label',
        'desc': 'This is a test description.',
        'items': [{
            'name': 'same_item',
            'value': '1',
            'caption': 'caption_a',
        }],
    }
    enum_policy_b = {
        'name': 'EnumPolicy.B',
        'type': 'string-enum',
        'caption': 'Enum policy B caption',
        'label': 'Enum policy B label',
        'desc': 'This is a test description.',
        'items': [{
            'name': 'same_item',
            'value': '2',
            'caption': 'caption_b',
        }],
    }
    self._InitWriterForAddingPolicies(self.writer, enum_policy_a)
    self.writer.WritePolicy(enum_policy_a)
    self.writer.WritePolicy(enum_policy_b)
    # Assert generated string elements.
    output = self.GetXMLOfChildren(self.writer._string_table_elem)
    expected_output = (
        '<string id="EnumPolicy_A">Enum policy A caption</string>\n'
        '<string id="EnumPolicy_A_Explain">'
        'This is a test description.</string>\n'
        '<string id="EnumPolicy_A_same_item">caption_a</string>\n'
        '<string id="EnumPolicy_B">Enum policy B caption</string>\n'
        '<string id="EnumPolicy_B_Explain">'
        'This is a test description.</string>\n'
        '<string id="EnumPolicy_B_same_item">caption_b</string>\n')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = ('<presentation id="EnumPolicy.A">\n'
                       '  <dropdownList refId="EnumPolicy.A">'
                       'Enum policy A label</dropdownList>\n'
                       '</presentation>\n'
                       '<presentation id="EnumPolicy.B">\n'
                       '  <dropdownList refId="EnumPolicy.B">'
                       'Enum policy B label</dropdownList>\n'
                       '</presentation>')
    self.AssertXMLEquals(output, expected_output)


if __name__ == '__main__':
  unittest.main()

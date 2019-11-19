#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for writers.chromeos_adml_writer."""

import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

from writers import chromeos_adml_writer
from writers import adml_writer_unittest
from writers.admx_writer import AdmxElementType


class ChromeOsAdmlWriterUnittest(adml_writer_unittest.AdmlWriterUnittest):

  # Overridden.
  def _GetWriter(self, config):
    return chromeos_adml_writer.GetWriter(config)

  # Overridden
  def GetCategory(self):
    return "cros_test_category"

  # Overridden
  def GetCategoryString(self):
    return "CrOSTestCategory"

  # Overridden.
  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Chrome OS.
    self.assertTrue(
        self.writer.IsPolicySupported({
            'supported_on': [{
                'platforms': ['chrome_os', 'zzz']
            }, {
                'platforms': ['aaa']
            }]
        }))
    self.assertFalse(
        self.writer.IsPolicySupported({
            'supported_on': [{
                'platforms': ['win', 'mac', 'linux']
            }, {
                'platforms': ['aaa']
            }]
        }))

  def testOnlySupportsAdPolicies(self):
    # Tests whether only Active Directory managed policies are supported (Google
    # cloud only managed polices are not put in the ADMX file).
    policy = {
        'name':
            'PolicyName',
        'supported_on': [{
            'product': 'chrome_os',
            'platforms': ['chrome_os'],
            'since_version': '8',
            'until_version': '',
        }],
    }
    self.assertTrue(self.writer.IsPolicySupported(policy))

    policy['supported_chrome_os_management'] = ['google_cloud']
    self.assertFalse(self.writer.IsPolicySupported(policy))

    policy['supported_chrome_os_management'] = ['active_directory']
    self.assertTrue(self.writer.IsPolicySupported(policy))

  # Overridden.
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
        'DictionaryPolicyStub\n</string>\n'
        '<string id="DictionaryPolicyStub_Legacy">'
        'Dictionary policy label (deprecated)</string>')
    self.AssertXMLEquals(output, expected_output)
    # Assert generated presentation elements.
    output = self.GetXMLOfChildren(self.writer._presentation_table_elem)
    expected_output = (
        '<presentation id="DictionaryPolicyStub">\n'
        '  <textBox refId="DictionaryPolicyStub_Legacy">\n'
        '    <label>Dictionary policy label (deprecated)</label>\n'
        '  </textBox>\n'
        '  <multiTextBox defaultHeight="8" refId="DictionaryPolicyStub">'
        'Dictionary policy label</multiTextBox>\n'
        '</presentation>')
    self.AssertXMLEquals(output, expected_output)


if __name__ == '__main__':
  unittest.main()

#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit tests for writers.reg_writer'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest

from writers import writer_unittest_common


class RegWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for RegWriter.'''

  NEWLINE = '\r\n'

  def CompareOutputs(self, output, expected_output):
    '''Compares the output of the reg_writer with its expected output.

    Args:
      output: The output of the reg writer.
      expected_output: The expected output.

    Raises:
      AssertionError: if the two strings are not equivalent.
    '''
    self.assertEquals(output.strip(), expected_output.strip())

  def testEmpty(self):
    # Test the handling of an empty policy list.
    policy_json = '''
        {
          "policy_definitions": [],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {}
        }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
    }, 'reg')
    expected_output = 'Windows Registry Editor Version 5.00'
    self.CompareOutputs(output, expected_output)

  def testEmptyVersion(self):
    # Test the handling of an empty policy list.
    policy_json = '''
        {
          "policy_definitions": [],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {}
        }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'version': '39.0.0.0'
    }, 'reg')
    expected_output = ('Windows Registry Editor Version 5.00\r\n'
                       '; chromium version: 39.0.0.0\r\n')
    self.CompareOutputs(output, expected_output)

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "MainPolicy",
              "type": "main",
              "features": { "can_be_recommended": True },
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": True
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome]',
        '"MainPolicy"=dword:00000001', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome\\Recommended]',
        '"MainPolicy"=dword:00000001'
    ])
    self.CompareOutputs(output, expected_output)

  def testRecommendedMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "MainPolicy",
              "type": "main",
              "features": {
                "can_be_recommended": True,
                "can_be_mandatory": False
              },
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": True
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome\\Recommended]',
        '"MainPolicy"=dword:00000001'
    ])
    self.CompareOutputs(output, expected_output)

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "StringPolicy",
              "type": "string",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": "hello, world! \\\" \\\\"
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"StringPolicy"="hello, world! \\\" \\\\"'
    ])
    self.CompareOutputs(output, expected_output)

  def testIntPolicy(self):
    # Tests a policy group with a single policy of type 'int'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "IntPolicy",
              "type": "int",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": 26
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"IntPolicy"=dword:0000001a'
    ])
    self.CompareOutputs(output, expected_output)

  def testIntEnumPolicy(self):
    # Tests a policy group with a single policy of type 'int-enum'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "EnumPolicy",
              "type": "int-enum",
              "caption": "",
              "desc": "",
              "items": [
                {"name": "ProxyServerDisabled", "value": 0, "caption": ""},
                {"name": "ProxyServerAutoDetect", "value": 1, "caption": ""},
              ],
              "supported_on": ["chrome.win:8-"],
              "example_value": 1
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome]',
        '"EnumPolicy"=dword:00000001'
    ])
    self.CompareOutputs(output, expected_output)

  def testStringEnumPolicy(self):
    # Tests a policy group with a single policy of type 'string-enum'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "EnumPolicy",
              "type": "string-enum",
              "caption": "",
              "desc": "",
              "items": [
                {"name": "ProxyServerDisabled", "value": "one", "caption": ""},
                {"name": "ProxyServerAutoDetect", "value": "two","caption": ""},
              ],
              "supported_on": ["chrome.win:8-"],
              "example_value": "two"
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_google_chrome': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Google\\Chrome]',
        '"EnumPolicy"="two"'
    ])
    self.CompareOutputs(output, expected_output)

  def testListPolicy(self):
    # Tests a policy group with a single policy of type 'list'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "ListPolicy",
              "type": "list",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.linux:8-"],
              "example_value": ["foo", "bar"]
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium\\ListPolicy]',
        '"1"="foo"', '"2"="bar"'
    ])

  def testStringEnumListPolicy(self):
    # Tests a policy group with a single policy of type 'string-enum-list'.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "ListPolicy",
              "type": "string-enum-list",
              "caption": "",
              "desc": "",
              "items": [
                {"name": "ProxyServerDisabled", "value": "foo", "caption": ""},
                {"name": "ProxyServerAutoDetect", "value": "bar","caption": ""},
              ],
              "supported_on": ["chrome.linux:8-"],
              "example_value": ["foo", "bar"]
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium\\ListPolicy]',
        '"1"="foo"', '"2"="bar"'
    ])

  def testDictionaryPolicy(self):
    # Tests a policy group with a single policy of type 'dict'.
    example = {
        'bool': True,
        'dict': {
            'a': 1,
            'b': 2,
        },
        'int': 10,
        'list': [1, 2, 3],
        'string': 'abc',
    }
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "DictionaryPolicy",
              "type": "dict",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": ''' + str(example) + '''
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"DictionaryPolicy"="{\\"bool\\": true, '
        '\\"dict\\": {\\"a\\": 1, \\"b\\": 2}, \\"int\\": 10, '
        '\\"list\\": [1, 2, 3], \\"string\\": \\"abc\\"}"'
    ])
    self.CompareOutputs(output, expected_output)

  def testExternalPolicy(self):
    # Tests a policy group with a single policy of type 'external'.
    example = {
        'url': "https://example.com/avatar.jpg",
        'hash': "deadbeef",
    }
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "ExternalPolicy",
              "type": "external",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": %s
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }''' % str(example)
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]',
        '"ExternalPolicy"="{\\"hash\\": \\"deadbeef\\", '
        '\\"url\\": \\"https://example.com/avatar.jpg\\"}"'
    ])
    self.CompareOutputs(output, expected_output)

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Windows, so it shouldn't
    # be included in the .REG file.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "NonWindowsPolicy",
              "type": "list",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.mac:8-"],
              "example_value": ["a"]
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join(
        ['Windows Registry Editor Version 5.00'])
    self.CompareOutputs(output, expected_output)

  def testPolicyGroup(self):
    # Tests a policy group that has more than one policies.
    policy_json = '''
        {
          "policy_definitions": [
            {
              "name": "Group1",
              "type": "group",
              "caption": "",
              "desc": "",
              "policies": ["Policy1", "Policy2"],
            },
            {
              "name": "Policy1",
              "type": "list",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": ["a", "b"]
            },
            {
              "name": "Policy2",
              "type": "string",
              "caption": "",
              "desc": "",
              "supported_on": ["chrome.win:8-"],
              "example_value": "c"
            },
          ],
          "policy_atomic_group_definitions": [],
          "placeholders": [],
          "messages": {},
        }'''
    output = self.GetOutput(policy_json, {'_chromium': '1'}, 'reg')
    expected_output = self.NEWLINE.join([
        'Windows Registry Editor Version 5.00', '',
        '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium]', '"Policy2"="c"',
        '', '[HKEY_LOCAL_MACHINE\\Software\\Policies\\Chromium\\Policy1]',
        '"1"="a"', '"2"="b"'
    ])
    self.CompareOutputs(output, expected_output)


if __name__ == '__main__':
  unittest.main()

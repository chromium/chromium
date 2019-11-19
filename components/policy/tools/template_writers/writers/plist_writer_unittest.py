#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit tests for writers.plist_writer'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest

from writers import writer_unittest_common


class PListWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for PListWriter.'''

  def _GetExpectedOutputs(self, product_name, bundle_id, policies):
    '''Substitutes the variable parts into a plist template. The result
    of this function can be used as an expected result to test the output
    of PListWriter.

    Args:
      product_name: The name of the product, normally Chromium or Google Chrome.
      bundle_id: The mac bundle id of the product.
      policies: The list of policies.

    Returns:
      The text of a plist template with the variable parts substituted.
    '''
    return '''
<?xml version="1.0" ?>
<!DOCTYPE plist  PUBLIC '-//Apple//DTD PLIST 1.0//EN'  'http://www.apple.com/DTDs/PropertyList-1.0.dtd'>
<plist version="1">
  <dict>
    <key>pfm_name</key>
    <string>%s</string>
    <key>pfm_description</key>
    <string/>
    <key>pfm_title</key>
    <string/>
    <key>pfm_version</key>
    <real>1</real>
    <key>pfm_domain</key>
    <string>%s</string>
    <key>pfm_subkeys</key>
    %s
  </dict>
</plist>''' % (product_name, bundle_id, policies)

  def _GetExpectedOutputsWithVersion(self, product_name, bundle_id, policies,
                                     version):
    '''Substitutes the variable parts into a plist template. The result
    of this function can be used as an expected result to test the output
    of PListWriter.

    Args:
      product_name: The name of the product, normally Chromium or Google Chrome.
      bundle_id: The mac bundle id of the product.
      policies: The list of policies.

    Returns:
      The text of a plist template with the variable parts substituted.
    '''
    return '''
<?xml version="1.0" ?>
<!DOCTYPE plist  PUBLIC '-//Apple//DTD PLIST 1.0//EN'  'http://www.apple.com/DTDs/PropertyList-1.0.dtd'>
<plist version="1">
  <dict>
    <key>pfm_name</key>
    <string>%s</string>
    <key>pfm_description</key>
    <string/>
    <key>pfm_title</key>
    <string/>
    <key>pfm_version</key>
    <real>1</real>
    <key>pfm_domain</key>
    <string>%s</string>
    <key>pfm_subkeys</key>
    %s
  </dict>
  <!--%s-->
</plist>''' % (product_name, bundle_id, policies, version)

  def testEmpty(self):
    # Test PListWriter in case of empty polices.
    policy_json = '''
      {
        'policy_definitions': [],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''

    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs('Chromium', 'com.example.Test',
                                               '<array/>')
    self.assertEquals(output.strip(), expected_output.strip())

  def testEmptyVersion(self):
    # Test PListWriter in case of empty polices.
    policy_json = '''
      {
        'policy_definitions': [],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''

    output = self.GetOutput(
        policy_json, {
            '_chromium': '1',
            'mac_bundle_id': 'com.example.Test',
            'version': '39.0.0.0'
        }, 'plist')
    expected_output = self._GetExpectedOutputsWithVersion(
        'Chromium', 'com.example.Test', '<array/>',
        'chromium version: 39.0.0.0')
    self.assertEquals(output.strip(), expected_output.strip())

  def testMainPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'MainGroup',
            'type': 'group',
            'policies': ['MainPolicy'],
            'desc': '',
            'caption': '',
          },
          {
            'name': 'MainPolicy',
            'type': 'main',
            'desc': '',
            'caption': '',
            'supported_on': ['chrome.mac:8-'],
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {}
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>MainPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>boolean</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testRecommendedPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'MainGroup',
            'type': 'group',
            'policies': ['MainPolicy'],
            'desc': '',
            'caption': '',
          },
          {
            'name': 'MainPolicy',
            'type': 'main',
            'desc': '',
            'caption': '',
            'features': {
              'can_be_recommended' : True
            },
            'supported_on': ['chrome.mac:8-'],
           },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {}
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>MainPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user</string>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>boolean</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testRecommendedOnlyPolicy(self):
    # Tests a policy group with a single policy of type 'main'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'MainGroup',
            'type': 'group',
            'policies': ['MainPolicy'],
            'desc': '',
            'caption': '',
          },
          {
            'name': 'MainPolicy',
            'type': 'main',
            'desc': '',
            'caption': '',
            'features': {
              'can_be_recommended' : True,
              'can_be_mandatory' : False
            },
            'supported_on': ['chrome.mac:8-'],
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {}
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>MainPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user</string>
        </array>
        <key>pfm_type</key>
        <string>boolean</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testStringPolicy(self):
    # Tests a policy group with a single policy of type 'string'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'StringGroup',
            'type': 'group',
            'desc': '',
            'caption': '',
            'policies': ['StringPolicy'],
          },
          {
            'name': 'StringPolicy',
            'type': 'string',
            'supported_on': ['chrome.mac:8-'],
            'desc': '',
            'caption': '',
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>StringPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>string</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testListPolicy(self):
    # Tests a policy group with a single policy of type 'list'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'ListGroup',
            'type': 'group',
            'desc': '',
            'caption': '',
            'policies': ['ListPolicy'],
          },
          {
            'name': 'ListPolicy',
            'type': 'list',
            'schema': {
              'type': 'array',
              'items': { 'type': 'string' },
             },
            'supported_on': ['chrome.mac:8-'],
            'desc': '',
            'caption': '',
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>ListPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>array</string>
        <key>pfm_subkeys</key>
        <array>
          <dict>
            <key>pfm_type</key>
            <string>string</string>
          </dict>
        </array>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testStringEnumListPolicy(self):
    # Tests a policy group with a single policy of type 'string-enum-list'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'ListGroup',
            'type': 'group',
            'desc': '',
            'caption': '',
            'policies': ['ListPolicy'],
          },
          {
            'name': 'ListPolicy',
            'type': 'string-enum-list',
            'schema': {
              'type': 'array',
              'items': { 'type': 'string' },
            },
            'items': [
              {'name': 'ProxyServerDisabled', 'value': 'one', 'caption': ''},
              {'name': 'ProxyServerAutoDetect', 'value': 'two', 'caption': ''},
            ],
            'supported_on': ['chrome.mac:8-'],
            'supported_on': ['chrome.mac:8-'],
            'desc': '',
            'caption': '',
          }
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>ListPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>array</string>
        <key>pfm_subkeys</key>
        <array>
          <dict>
            <key>pfm_type</key>
            <string>string</string>
          </dict>
        </array>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testIntPolicy(self):
    # Tests a policy group with a single policy of type 'int'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'IntGroup',
            'type': 'group',
            'caption': '',
            'desc': '',
            'policies': ['IntPolicy'],
          },
          {
            'name': 'IntPolicy',
            'type': 'int',
            'caption': '',
            'desc': '',
            'supported_on': ['chrome.mac:8-'],
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>IntPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>integer</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testIntEnumPolicy(self):
    # Tests a policy group with a single policy of type 'int-enum'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'EnumGroup',
            'type': 'group',
            'caption': '',
            'desc': '',
            'policies': ['EnumPolicy'],
          },
          {
            'name': 'EnumPolicy',
            'type': 'int-enum',
            'desc': '',
            'caption': '',
            'items': [
              {'name': 'ProxyServerDisabled', 'value': 0, 'caption': ''},
              {'name': 'ProxyServerAutoDetect', 'value': 1, 'caption': ''},
            ],
            'supported_on': ['chrome.mac:8-'],
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'mac_bundle_id': 'com.example.Test2'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Google_Chrome', 'com.example.Test2', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>EnumPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>integer</string>
        <key>pfm_range_list</key>
        <array>
          <integer>0</integer>
          <integer>1</integer>
        </array>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testStringEnumPolicy(self):
    # Tests a policy group with a single policy of type 'string-enum'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'EnumGroup',
            'type': 'group',
            'caption': '',
            'desc': '',
            'policies': ['EnumPolicy'],
          },
          {
            'name': 'EnumPolicy',
            'type': 'string-enum',
            'desc': '',
            'caption': '',
            'items': [
              {'name': 'ProxyServerDisabled', 'value': 'one', 'caption': ''},
              {'name': 'ProxyServerAutoDetect', 'value': 'two', 'caption': ''},
            ],
            'supported_on': ['chrome.mac:8-'],
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'mac_bundle_id': 'com.example.Test2'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Google_Chrome', 'com.example.Test2', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>EnumPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>string</string>
        <key>pfm_range_list</key>
        <array>
          <string>one</string>
          <string>two</string>
        </array>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testDictionaryPolicy(self):
    # Tests a policy group with a single policy of type 'dict'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'DictionaryGroup',
            'type': 'group',
            'desc': '',
            'caption': '',
            'policies': ['DictionaryPolicy'],
          },
          {
            'name': 'DictionaryPolicy',
            'type': 'dict',
            'supported_on': ['chrome.mac:8-'],
            'desc': '',
            'caption': '',
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>DictionaryPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>dictionary</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testExternalPolicy(self):
    # Tests a policy group with a single policy of type 'dict'.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'ExternalGroup',
            'type': 'group',
            'desc': '',
            'caption': '',
            'policies': ['ExternalPolicy'],
          },
          {
            'name': 'ExternalPolicy',
            'type': 'external',
            'supported_on': ['chrome.mac:8-'],
            'desc': '',
            'caption': '',
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_chromium': '1',
        'mac_bundle_id': 'com.example.Test'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Chromium', 'com.example.Test', '''<array>
      <dict>
        <key>pfm_name</key>
        <string>ExternalPolicy</string>
        <key>pfm_description</key>
        <string/>
        <key>pfm_title</key>
        <string/>
        <key>pfm_targets</key>
        <array>
          <string>user-managed</string>
        </array>
        <key>pfm_type</key>
        <string>dictionary</string>
      </dict>
    </array>''')
    self.assertEquals(output.strip(), expected_output.strip())

  def testNonSupportedPolicy(self):
    # Tests a policy that is not supported on Mac, so it shouldn't
    # be included in the plist file.
    policy_json = '''
      {
        'policy_definitions': [
          {
            'name': 'NonMacGroup',
            'type': 'group',
            'caption': '',
            'desc': '',
            'policies': ['NonMacPolicy'],
          },
          {
            'name': 'NonMacPolicy',
            'type': 'string',
            'supported_on': ['chrome.linux:8-', 'chrome.win:7-'],
            'caption': '',
            'desc': '',
          },
        ],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {},
      }'''
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'mac_bundle_id': 'com.example.Test2'
    }, 'plist')
    expected_output = self._GetExpectedOutputs(
        'Google_Chrome', 'com.example.Test2', '''<array/>''')
    self.assertEquals(output.strip(), expected_output.strip())


if __name__ == '__main__':
  unittest.main()

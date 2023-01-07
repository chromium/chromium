#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest

import copy
import json
from writers import writer_unittest_common


def _JsonFormat(input):
  return json.dumps(input, indent=2, sort_keys=True, separators=(',', ': '))


class JamfWriterUnitTests(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for JamfWriter.'''

  doc_url = 'https://chromeenterprise.google/policies/'

  def _GetTestPolicyTemplate(self, policy_name, policy_type, schema_type,
                             policy_caption):
    template = {
        'policy_definitions': [{
            'name':
            policy_name,
            'id':
            1,
            'type':
            policy_type,
            'supported_on': ['chrome.mac:*-'],
            'caption':
            policy_caption,
            'desc':
            '',
            'items': [{
                'name': 'title1',
                'value': 1,
                'caption': '',
                'type': 'integer'
            }],
            'schema': {
                'type': schema_type,
                'id': 'enumid',
                'properties' if schema_type == 'object' else 'items': {
                    'title':
                    'title_obj' if schema_type == 'object' else 'title_array',
                    'type': 'integer'
                },
            }
        }],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {
            'doc_policy_documentation': {
                'text': 'Documentation for policy'
            }
        },
    }
    return _JsonFormat(template)

  def _GetExpectedOutput(self, policy_name, policy_type, policy_caption,
                         initial_type, version):
    output = {
        'description': 'Google Chrome',
        'options': {
            'remove_empty_properties': True
        },
        'properties': {
            policy_name: {
                'description':
                policy_caption,
                'title':
                policy_name,
                'type':
                policy_type,
                'links': [{
                    'rel': 'Documentation for policy',
                    'href': self.doc_url + '#' + policy_name
                }]
            }
        },
        'title': 'com.google.chrome.ios',
        'version': version
    }
    if initial_type == 'int-enum' or initial_type == 'string-enum':
      output['properties'][policy_name]['enum'] = [1]
      output['properties'][policy_name]['options'] = {'enum_titles': ['title1']}
    if initial_type == 'string-enum-list' or initial_type == 'int-enum-list':
      output['properties'][policy_name]['items'] = {
          'type': 'integer',
          'enum': [1],
          'options': {
              'enum_titles': ['title1']
          }
      }
    elif policy_type == 'array':
      output['properties'][policy_name]['items'] = {
          'type': 'integer',
          'title': 'title_array'
      }
    elif initial_type == 'dict':
      output['properties'][policy_name]['properties'] = {
          'type': 'integer',
          'title': 'title_obj'
      }

    return _JsonFormat(output)

  def testStringPolicy(self):
    policy_json = self._GetTestPolicyTemplate('stringPolicy', 'string', '',
                                              'A string policy')
    expected = self._GetExpectedOutput('stringPolicy', 'string',
                                       'A string policy', 'string', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testIntPolicy(self):
    policy_json = self._GetTestPolicyTemplate('intPolicy', 'int', '',
                                              'An int policy')
    expected = self._GetExpectedOutput('intPolicy', 'integer', 'An int policy',
                                       'int', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testIntPolicyWithMinAndMax(self):
    template = {
        'policy_definitions': [{
            'name': 'intPolicyWithMinAndMax',
            'id': 1,
            'type': 'int',
            'supported_on': ['chrome.mac:*-'],
            'caption': 'An int policy with min and max',
            'desc': '',
            'schema': {
                'type': 'int',
                'minimum': 0,
                'maximum': 10
            }
        }],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {
            'doc_policy_documentation': {
                'text': 'Documentation for policy'
            }
        }
    }
    policy_json = _JsonFormat(template)

    expected = {
        'description': 'Google Chrome',
        'options': {
            'remove_empty_properties': True
        },
        'properties': {
            'intPolicyWithMinAndMax': {
                'description':
                'An int policy with min and max',
                'maximum':
                10,
                'minimum':
                0,
                'title':
                'intPolicyWithMinAndMax',
                'type':
                'integer',
                'links': [{
                    'rel': 'Documentation for policy',
                    'href': self.doc_url + '#' + 'intPolicyWithMinAndMax'
                }]
            }
        },
        'title': 'com.google.chrome.ios',
        'version': '83'
    }
    expected_json = _JsonFormat(expected)

    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected_json.strip())

  def testIntEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('intPolicy', 'int-enum', '',
                                              'An int-enum policy')
    expected = self._GetExpectedOutput('intPolicy', 'integer',
                                       'An int-enum policy', 'int-enum', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumPolicy(self):
    policy_json = self._GetTestPolicyTemplate('stringPolicy', 'string-enum', '',
                                              'A string-enum policy')
    expected = self._GetExpectedOutput('stringPolicy', 'string',
                                       'A string-enum policy', 'string-enum',
                                       '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testStringEnumListPolicy(self):
    policy_json = self._GetTestPolicyTemplate('stringPolicy',
                                              'string-enum-list', '',
                                              'A string-enum-list policy')
    expected = self._GetExpectedOutput('stringPolicy', 'array',
                                       'A string-enum-list policy',
                                       'string-enum-list', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testBooleanPolicy(self):
    policy_json = self._GetTestPolicyTemplate('booleanPolicy', 'main', '',
                                              'A boolean policy')
    expected = self._GetExpectedOutput('booleanPolicy', 'boolean',
                                       'A boolean policy', 'main', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testListPolicy(self):
    policy_json = self._GetTestPolicyTemplate('listPolicy', 'list', '',
                                              'A list policy')
    expected = self._GetExpectedOutput('listPolicy', 'array', 'A list policy',
                                       'list', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testDictPolicy(self):
    policy_json = self._GetTestPolicyTemplate('dictPolicy', 'dict', 'object',
                                              'A dict policy')
    expected = self._GetExpectedOutput('dictPolicy', 'object', 'A dict policy',
                                       'dict', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testArrayDictPolicy(self):
    policy_json = self._GetTestPolicyTemplate('dictPolicy', 'dict', 'array',
                                              'A dict policy')
    expected = self._GetExpectedOutput('dictPolicy', 'array', 'A dict policy',
                                       'dict', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())

  def testNestedArrayDict(self):
    template = {
        'policy_definitions': [{
            'name': 'name',
            'id': 1,
            'type': 'dict',
            'supported_on': ['chrome.mac:*-'],
            'caption': 'caption',
            'desc': '',
            'schema': {
                'type': 'array',
                'items': {
                    'title': 'title2',
                    'id': 'id',
                    'type': 'object',
                    'properties': {
                        'name': 'name',
                        'children': {
                            'type': 'array',
                            'items': {
                                '$ref': 'id'
                            }
                        }
                    }
                }
            }
        }],
        'policy_atomic_group_definitions': [],
        'placeholders': [],
        'messages': {
            'doc_policy_documentation': {
                'text': 'Documentation for policy'
            }
        }
    }
    policy_json = _JsonFormat(template)

    expected = {
        'description': 'Google Chrome',
        'options': {
            'remove_empty_properties': True
        },
        'properties': {
            'name': {
                'description':
                'caption',
                'title':
                'name',
                'type':
                'array',
                'items': {
                    'title': 'title2',
                    'id': 'id',
                    'type': 'object',
                    'properties': {
                        'name': 'name'
                    }
                },
                'links': [{
                    'rel': 'Documentation for policy',
                    'href': self.doc_url + '#' + 'name'
                }]
            }
        },
        'title': 'com.google.chrome.ios',
        'version': '83'
    }

    for i in range(0, 5):
      expected['properties']['name']['items']['properties']['children'] = {
          'type': 'array',
          'items': copy.deepcopy(expected['properties']['name']['items'])
      }

    output_expected = _JsonFormat(expected)
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), output_expected.strip())

  def testExternalPolicy(self):
    policy_json = self._GetTestPolicyTemplate('externalPolicy', 'external', '',
                                              'A external policy')
    expected = self._GetExpectedOutput('externalPolicy', 'object',
                                       'A external policy', 'external', '83')
    output = self.GetOutput(policy_json, {
        '_google_chrome': '1',
        'version': '83.0.4089.0',
        'doc_url': self.doc_url
    }, 'jamf')
    self.assertEquals(output.strip(), expected.strip())


if __name__ == '__main__':
  unittest.main()

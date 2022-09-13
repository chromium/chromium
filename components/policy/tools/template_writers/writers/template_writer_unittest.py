#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit tests for writers.template_writer'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest

from writers import template_writer

POLICY_DEFS = [
    {
        'name': 'zp',
        'type': 'string',
        'caption': 'a1',
        'supported_on': []
    },
    {
        'type':
            'group',
        'caption':
            'z_group1_caption',
        'name':
            'group1',
        'policies': [{
            'name': 'z0',
            'type': 'string',
            'supported_on': []
        }, {
            'name': 'a0',
            'type': 'string',
            'supported_on': []
        }]
    },
    {
        'type': 'group',
        'caption': 'b_group2_caption',
        'name': 'group2',
        'policies': [{
            'name': 'q',
            'type': 'string',
            'supported_on': []
        }],
    }, {
        'name': 'ap',
        'type': 'string',
        'caption': 'a2',
        'supported_on': []
    }
]

GROUP_FIRST_SORTED_POLICY_DEFS = [
    {
        'type': 'group',
        'caption': 'b_group2_caption',
        'name': 'group2',
        'policies': [{
            'name': 'q',
            'type': 'string',
            'supported_on': []
        }],
    },
    {
        'type':
            'group',
        'caption':
            'z_group1_caption',
        'name':
            'group1',
        'policies': [{
            'name': 'z0',
            'type': 'string',
            'supported_on': []
        }, {
            'name': 'a0',
            'type': 'string',
            'supported_on': []
        }]
    },
    {
        'name': 'ap',
        'type': 'string',
        'caption': 'a2',
        'supported_on': []
    },
    {
        'name': 'zp',
        'type': 'string',
        'caption': 'a1',
        'supported_on': []
    },
]

IGNORE_GROUPS_SORTED_POLICY_DEFS = [
    {
        'name': 'a0',
        'type': 'string',
        'supported_on': []
    },
    {
        'name': 'ap',
        'type': 'string',
        'caption': 'a2',
        'supported_on': []
    },
    {
        'name': 'q',
        'type': 'string',
        'supported_on': []
    },
    {
        'name': 'z0',
        'type': 'string',
        'supported_on': []
    },
    {
        'name': 'zp',
        'type': 'string',
        'caption': 'a1',
        'supported_on': []
    },
]


class TemplateWriterUnittests(unittest.TestCase):
  '''Unit tests for templater_writer.py.'''

  def _IsPolicySupported(self,
                         platform,
                         version,
                         policy,
                         writer=template_writer.TemplateWriter):
    tw = writer([platform], {'major_version': version})
    if platform != '*':
      self.assertEqual(
          tw.IsPolicySupported(policy),
          tw.IsPolicyOrItemSupportedOnPlatform(policy, platform))
    return tw.IsPolicySupported(policy)

  def testSortingGroupsFirst(self):
    tw = template_writer.TemplateWriter(None, None)
    sorted_list = tw.SortPoliciesGroupsFirst(POLICY_DEFS)
    self.assertEqual(sorted_list, GROUP_FIRST_SORTED_POLICY_DEFS)

  def testSortingIgnoreGroups(self):
    tw = template_writer.TemplateWriter(None, None)
    sorted_list = tw.FlattenGroupsAndSortPolicies(POLICY_DEFS)
    self.assertEqual(sorted_list, IGNORE_GROUPS_SORTED_POLICY_DEFS)

  def testPoliciesIsNotSupported(self):
    tw = template_writer.TemplateWriter(None, None)
    self.assertFalse(tw.IsPolicySupported({'deprecated': True}))
    self.assertFalse(tw.IsPolicySupported({'features': {'cloud_only': True}}))
    self.assertFalse(tw.IsPolicySupported({'features': {
        'internal_only': True
    }}))

  def testFuturePoliciesSupport(self):
    class FutureWriter(template_writer.TemplateWriter):
      def IsFuturePolicySupported(self, policy):
        return True

    expected_request_for_all_platforms = [[False, True, True],
                                          [True, True, True]]
    expected_request_for_all_win = [[False, False, True], [True, True, True]]
    for i, writer in enumerate([template_writer.TemplateWriter, FutureWriter]):
      for j, policy in enumerate([{
          'supported_on': [],
          'future_on': [{
              'product': 'chrome',
              'platform': 'win'
          }, {
              'product': 'chrome',
              'platform': 'mac'
          }]
      }, {
          'supported_on': [{
              'product': 'chrome',
              'platform': 'mac'
          }],
          'future_on': [{
              'product': 'chrome',
              'platform': 'win'
          }]
      }, {
          'supported_on': [{
              'product': 'chrome',
              'platform': 'win'
          }, {
              'product': 'chrome',
              'platform': 'mac'
          }],
          'future_on': []
      }]):
        self.assertEqual(expected_request_for_all_platforms[i][j],
                         self._IsPolicySupported('*', None, policy, writer))
        self.assertEqual(
            expected_request_for_all_win[i][j],
            self._IsPolicySupported('win', None, policy, writer),
        )

  def testPoliciesIsSupportedOnCertainVersion(self):
    platform = 'win'
    policy = {
        'supported_on': [{
            'platform': 'win',
            'since_version': '11',
            'until_version': '12'
        }]
    }
    self.assertFalse(self._IsPolicySupported(platform, 10, policy))
    self.assertTrue(self._IsPolicySupported(platform, 11, policy))
    self.assertTrue(self._IsPolicySupported(platform, 12, policy))
    self.assertFalse(self._IsPolicySupported(platform, 13, policy))

    policy = {
        'supported_on': [{
            'platform': 'win',
            'since_version': '11',
            'until_version': ''
        }]
    }
    self.assertFalse(self._IsPolicySupported(platform, 10, policy))
    self.assertTrue(self._IsPolicySupported(platform, 11, policy))
    self.assertTrue(self._IsPolicySupported(platform, 12, policy))
    self.assertTrue(self._IsPolicySupported(platform, 13, policy))

  def testPoliciesIsSupportedOnMulitplePlatform(self):
    policy = {
        'supported_on': [{
            'platform': 'win',
            'since_version': '12',
            'until_version': ''
        }, {
            'platform': 'mac',
            'since_version': '11',
            'until_version': ''
        }]
    }
    self.assertFalse(self._IsPolicySupported('win', 11, policy))
    self.assertTrue(self._IsPolicySupported('mac', 11, policy))
    self.assertTrue(self._IsPolicySupported('*', 11, policy))
    self.assertFalse(self._IsPolicySupported('*', 10, policy))

  def testHasExpandedPolicyDescriptionForUrlSchema(self):
    policy = {'url_schema': 'https://example.com/details', 'type': 'list'}
    tw = template_writer.TemplateWriter(None, None)
    self.assertTrue(tw.HasExpandedPolicyDescription(policy))

  def testHasExpandedPolicyDescriptionForJSONPolicies(self):
    policy = {'name': 'PolicyName', 'type': 'dict'}
    tw = template_writer.TemplateWriter(None, None)
    self.assertTrue(tw.HasExpandedPolicyDescription(policy))

  def testGetExpandedPolicyDescriptionForUrlSchema(self):
    policy = {'type': 'integer', 'url_schema': 'https://example.com/details'}
    tw = template_writer.TemplateWriter(None, None)
    tw.messages = {
        'doc_schema_description_link': {
            'text': '''See $6'''
        },
    }
    expanded_description = tw.GetExpandedPolicyDescription(policy)
    self.assertEqual(expanded_description, 'See https://example.com/details')

  def testGetExpandedPolicyDescriptionForJSONPolicies(self):
    policy = {'name': 'PolicyName', 'type': 'dict'}
    tw = template_writer.TemplateWriter(None, None)
    tw.messages = {
        'doc_schema_description_link': {
            'text': '''See $6'''
        },
    }
    expanded_description = tw.GetExpandedPolicyDescription(policy)
    self.assertEqual(
        expanded_description,
        'See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=PolicyName'
    )


if __name__ == '__main__':
  unittest.main()

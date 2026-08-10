#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import syntax_check_policy_template_json


class SyntaxCheckPolicyTemplateUnittest(unittest.TestCase):
  '''Unit tests for syntax_check_policy_template_json.py'''
  def do_test(self,
              policy_list,
              current_version=1,
              known_features=['per_profile', 'dynamic_refresh'],
              expect_exception=False,
              warnings=0,
              errors=0):
    exception_raised = False
    try:
      schemas_by_id = {}
      checker = syntax_check_policy_template_json.PolicyTemplateChecker()
      checker.SetFeatures(known_features)
      checker.CheckPolicyDefinitions(policy_list, current_version,
                                     schemas_by_id)
    except Exception as e:
      exception_raised = True
      if not expect_exception:
        print(e)

    self.assertEqual(exception_raised, expect_exception,
                     'Exception expectation failed')
    if exception_raised:
      return
    self.assertEqual(len(checker.warnings), warnings,
                     'Warnings expectation failed: ' + str(checker.warnings))
    self.assertEqual(len(checker.errors), errors,
                     'Errors expectation failed: ' + str(checker.errors))

  def testCorrectPolicy(self):
    policy_list = [{
        'name':
        'TestName',
        'supported_on': ['chrome.*:1-'],
        'schema': {
            'type': 'boolean'
        },
        'type':
        'main',
        'desc':
        'test desc, when true, when false',
        'features': {
            'per_profile': True,
            'dynamic_refresh': True
        },
        'caption':
        'test caption',
        'owners': ['test@chromium.org'],
        'tags': [],
        'items': [{
            'value': False,
            'caption': 'false caption'
        }, {
            'value': True,
            'caption': 'true caption'
        }],
        'example_value':
        True,
        'default':
        False
    }]
    self.do_test(policy_list)

  def testEmptyPolicy(self):
    policy_list = [{}]
    self.do_test(policy_list, expect_exception=True)

  def testDefaultForEnterpriseUsersWithFutureOn(self):
    policy = {
        'name':
        'NewFutureOnPolicy',
        'future_on': ['chrome_os'],
        'default_for_enterprise_users':
        True,
        'schema': {
            'type': 'boolean'
        },
        'type':
        'main',
        'desc':
        'test desc, when true, when false',
        'features': {
            'per_profile': True,
            'dynamic_refresh': True
        },
        'caption':
        'test caption',
        'owners': ['test@chromium.org'],
        'tags': [],
        'items': [{
            'value': False,
            'caption': 'false'
        }, {
            'value': True,
            'caption': 'true'
        }],
        'example_value':
        True,
        'default':
        False,
    }
    policy_change_list = [{
        'policy': 'NewFutureOnPolicy',
        'old_policy': None,
        'new_policy': policy
    }]

    # Case 1: Unallowlisted policy with future_on on ChromeOS logs error.
    checker = syntax_check_policy_template_json.PolicyTemplateChecker()
    checker.SetFeatures(['per_profile', 'dynamic_refresh'])
    checker.CheckModifiedPolicies(policy_change_list, 1, {}, False)
    self.assertEqual(len(checker.errors), 1)
    self.assertIn('default_for_enterprise_users', checker.errors[0])
    self.assertIn('future_on', checker.errors[0])

    # Case 2: Allowlisted policy with future_on on ChromeOS logs no error.
    legacy_policy = dict(policy)
    legacy_policy['name'] = 'CastReceiverEnabled'
    legacy_policy_change_list = [{
        'policy': 'CastReceiverEnabled',
        'old_policy': None,
        'new_policy': legacy_policy
    }]
    checker_legacy = syntax_check_policy_template_json.PolicyTemplateChecker()
    checker_legacy.SetFeatures(['per_profile', 'dynamic_refresh'])
    checker_legacy.CheckModifiedPolicies(legacy_policy_change_list, 1, {},
                                         False)
    self.assertEqual(len(checker_legacy.errors), 0)

    # Case 3: Non-ChromeOS future_on policy (e.g. fuchsia) logs no error.
    non_cros_policy = dict(policy)
    non_cros_policy['name'] = 'NonCrosFutureOnPolicy'
    non_cros_policy['future_on'] = ['fuchsia']
    non_cros_change_list = [{
        'policy': 'NonCrosFutureOnPolicy',
        'old_policy': None,
        'new_policy': non_cros_policy
    }]
    checker_non_cros = syntax_check_policy_template_json.PolicyTemplateChecker()
    checker_non_cros.SetFeatures(['per_profile', 'dynamic_refresh'])
    checker_non_cros.CheckModifiedPolicies(non_cros_change_list, 1, {}, False)
    self.assertEqual(len(checker_non_cros.errors), 0)


if __name__ == '__main__':
  unittest.main()

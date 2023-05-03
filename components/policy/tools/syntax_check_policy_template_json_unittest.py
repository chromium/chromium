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
        True
    }]
    self.do_test(policy_list)

  def testEmptyPolicy(self):
    policy_list = [{}]
    self.do_test(policy_list, expect_exception=True)


if __name__ == '__main__':
  unittest.main()

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import policy_templates as pt


class TestSensitivePolicyNoticeAppender(unittest.TestCase):

  def testContainsSensitivePolicyNotices_NoNotices(self):
    policy = {'desc': "Some text.\n More text."}

    self.assertFalse(pt._ContainsSensitivePolicyNotices(policy))


  def testContainsSensitivePolicyNotices_SingleNotice(self):
    policy = {
      'desc': f"Some text.\n{pt.SENSITIVE_POLICY_NOTICES['win']} More text."
    }

    self.assertTrue(pt._ContainsSensitivePolicyNotices(policy))


  def testContainsSensitivePolicyNotices_MultipleNotices(self):
    policy = {
      'desc' : (
        f"Some text.\n{pt.SENSITIVE_POLICY_NOTICES['win']}\n"
        f"{pt.SENSITIVE_POLICY_NOTICES['mac']}"
      )
    }

    self.assertTrue(pt._ContainsSensitivePolicyNotices(policy))


  def testAppendSensitivePolicyNotices_PlatformWithNoSensitiveNotice(self):
    policy_orig_desc = "Original desc."
    policy = {'desc': policy_orig_desc, 'supported_on': ['chrome.linux:80-']}

    pt._AppendSensitivePolicyNotices(policy)

    self.assertEqual(policy['desc'], policy_orig_desc)


  def testAppendSensitivePolicyNotices_SinglePlatform(self):
    policy = {'desc': "Original desc.", 'supported_on': ['chrome.win:80-']}

    pt._AppendSensitivePolicyNotices(policy)

    expected_desc = f"Original desc.\n\n{pt.SENSITIVE_POLICY_NOTICES['win']}"
    self.assertEqual(policy['desc'], expected_desc)


  def testAppendSensitivePolicyNotices_AppendMultiplePlatformsInOrder(self):
    policy = {
      'desc': "Original desc.",
      'supported_on': ['chrome.mac:80-'],
      'future_on': ['chrome.*:80-']
    }

    pt._AppendSensitivePolicyNotices(policy)

    expected_desc = (
      "Original desc.\n\n"
      f"{pt.SENSITIVE_POLICY_NOTICES['win']}\n\n"
      f"{pt.SENSITIVE_POLICY_NOTICES['mac']}")
    self.assertEqual(policy['desc'], expected_desc)


if __name__ == '__main__':
  unittest.main()

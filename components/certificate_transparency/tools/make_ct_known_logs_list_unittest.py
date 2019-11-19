#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import hashlib
import sys
import unittest
import make_ct_known_logs_list


def b64e(x):
  return base64.b64encode(x.encode())


class FormattingTest(unittest.TestCase):

  def testSplitAndHexifyBinData(self):
    bin_data = bytes(bytearray(range(32, 60)))
    expected_encoded_array = [
        ('"\\x20\\x21\\x22\\x23\\x24\\x25\\x26\\x27\\x28\\x29\\x2a'
         '\\x2b\\x2c\\x2d\\x2e\\x2f\\x30"'),
        '"\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\\x39\\x3a\\x3b"'
    ]
    self.assertEqual(
        make_ct_known_logs_list._split_and_hexify_binary_data(bin_data),
        expected_encoded_array)

    # This data should fit in exactly one line - 17 bytes.
    short_bin_data = bytes(bytearray(range(32, 49)))
    expected_short_array = [
        ('"\\x20\\x21\\x22\\x23\\x24\\x25\\x26\\x27\\x28\\x29\\x2a'
         '\\x2b\\x2c\\x2d\\x2e\\x2f\\x30"')
    ]
    self.assertEqual(
        make_ct_known_logs_list._split_and_hexify_binary_data(short_bin_data),
        expected_short_array)

    # This data should fit exactly in two lines - 34 bytes.
    two_line_data = bytes(bytearray(range(32, 66)))
    expected_two_line_data_array = [
        ('"\\x20\\x21\\x22\\x23\\x24\\x25\\x26\\x27\\x28\\x29\\x2a'
         '\\x2b\\x2c\\x2d\\x2e\\x2f\\x30"'),
        ('"\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\\x39\\x3a\\x3b'
         '\\x3c\\x3d\\x3e\\x3f\\x40\\x41"')
    ]
    self.assertEqual(
        make_ct_known_logs_list._split_and_hexify_binary_data(two_line_data),
        expected_two_line_data_array)

  def testGetLogIDsArray(self):
    log_ids = [b"def", b"abc", b"ghi"]
    expected_log_ids_code = [
        "// The list is sorted.\n", "const char kTestIDs[][4] = {\n",
        '    "\\x61\\x62\\x63",\n', '    "\\x64\\x65\\x66",\n',
        '    "\\x67\\x68\\x69"', "};\n\n"
    ]
    self.assertEqual(
        make_ct_known_logs_list._get_log_ids_array(log_ids, "kTestIDs"),
        expected_log_ids_code)

  def testToLogInfoStruct(self):
    log = {
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
        "dns": "dns.ct.example.com"
    }
    expected_loginfo = (
        '    {"\\x61\\x62\\x63",\n     3,\n     "Test Description"}')
    self.assertEqual(
        make_ct_known_logs_list._to_loginfo_struct(log), expected_loginfo)

  def testToLogInfoStructNoDNS(self):
    log = {
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
    }
    expected_loginfo = (
        '    {"\\x61\\x62\\x63",\n     3,\n     "Test Description"}')
    self.assertEqual(
        make_ct_known_logs_list._to_loginfo_struct(log), expected_loginfo)


class OperatorHandlingTest(unittest.TestCase):

  @classmethod
  def setUpClass(self):
    if sys.version_info.major == 2:
      self.assertCountEqual = self.assertItemsEqual

  def testCollectingLogIDsByOperator(self):
    logs = [{
        "name":
            "First",
        "email": ["first@email.com"],
        "logs": [{
            "log_id": b64e("a"),
            "state": {
                "qualified": {
                    "timestamp": "2019-04-09T02:49:10Z"
                }
            }
        },
                 {
                     "log_id": b64e("d"),
                     "state": {
                         "readonly": {
                             "timestamp": "2018-09-23T05:00:00Z"
                         }
                     }
                 }]
    },
            {
                "name":
                    "Second",
                "email": ["second@email.com"],
                "logs": [{
                    "log_id": b64e("b"),
                    "state": {
                        "rejected": {
                            "timestamp": "2018-01-12T00:00:00Z"
                        }
                    }
                }]
            },
            {
                "name":
                    "Third",
                "email": ["third@email.com"],
                "logs": [{
                    "log_id": b64e("c"),
                    "state": {
                        "usable": {
                            "timestamp": "2019-02-25T08:32:54Z"
                        }
                    }
                }]
            }]
    log_ids_1 = make_ct_known_logs_list._get_log_ids_for_operator(logs, "First")
    self.assertEqual(2, len(log_ids_1))
    self.assertCountEqual([b'a', b'd'], log_ids_1)
    log_ids_2 = make_ct_known_logs_list._get_log_ids_for_operator(
        logs, "Second")
    self.assertEqual(0, len(log_ids_2))
    log_ids_3 = make_ct_known_logs_list._get_log_ids_for_operator(logs, "Third")
    self.assertEqual(1, len(log_ids_3))
    self.assertCountEqual([b'c'], log_ids_3)


class DisqualifiedLogsHandlingTest(unittest.TestCase):

  def testCorrectlyIdentifiesDisqualifiedLog(self):
    self.assertTrue(
        make_ct_known_logs_list._is_log_disqualified({
            "state": {
                "retired": {
                    "timestamp": "2019-02-25T08:32:54Z"
                }
            }
        }))
    self.assertFalse(
        make_ct_known_logs_list._is_log_disqualified({
            "state": {
                "qualified": {
                    "timestamp": "2017-06-20T00:00:00Z"
                }
            }
        }))

  def testTranslatingToDisqualifiedLogDefinition(self):
    log = {
        "log_id": "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=",
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
        "state": {
            "retired": {
                "timestamp": "2019-02-25T08:32:54Z"
            }
        }
    }
    expected_disqualified_log_info = (
        '    {"\\xba\\x78\\x16\\xbf\\x8f\\x01\\xcf\\xea\\x41\\x41\\x40'
        '\\xde\\x5d\\xae\\x22\\x23\\xb0"\n     "\\x03\\x61\\xa3\\x96\\x17'
        '\\x7a\\x9c\\xb4\\x10\\xff\\x61\\xf2\\x00\\x15\\xad",\n    {"\\x61'
        '\\x62\\x63",\n     3,\n     "Test Description"'
        '},\n     '
        "base::TimeDelta::FromSeconds(1551083574)}")

    self.assertEqual(
        make_ct_known_logs_list._to_disqualified_loginfo_struct(log),
        expected_disqualified_log_info)

  def testSortingAndFilteringDisqualifiedLogs(self):
    logs = [{
        "state": {
            "retired": {
                "timestamp": "2018-12-12T00:00:00Z"
            }
        },
        "log_id": b64e("a")
    },
            {
                "state": {
                    "qualified": {
                        "timestamp": "2019-01-01T00:00:00Z"
                    }
                },
                "log_id": b64e("b")
            },
            {
                "state": {
                    "retired": {
                        "timestamp": "2019-01-01T00:00:00Z"
                    }
                },
                "log_id": b64e("c")
            },
            {
                "state": {
                    "retired": {
                        "timestamp": "2019-01-01T00:00:00Z"
                    }
                },
                "log_id": b64e("d")
            },
            {
                "state": {
                    "usable": {
                        "timestamp": "2018-01-01T00:00:00Z"
                    }
                },
                "log_id": b64e("e")
            }]
    disqualified_logs = make_ct_known_logs_list._sorted_disqualified_logs(logs)
    self.assertEqual(3, len(disqualified_logs))
    self.assertEqual(b64e("a"), disqualified_logs[0]["log_id"])
    self.assertEqual(b64e("c"), disqualified_logs[1]["log_id"])
    self.assertEqual(b64e("d"), disqualified_logs[2]["log_id"])


if __name__ == "__main__":
  unittest.main()

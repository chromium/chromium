#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
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
        '    {"\\x61\\x62\\x63",\n     3,\n     "Test Description",\n     "",'
        '\n     nullptr, 0}')
    self.assertEqual(
        make_ct_known_logs_list._to_loginfo_struct(log, 1), expected_loginfo)

  def testToLogInfoStructWithCurrentOperator(self):
    log = {
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
        "dns": "dns.ct.example.com"
    }
    # This is added separately instead of included in the json since it is done
    # separately in the generation flow too.
    log["current_operator"] = "Test Operator"
    expected_loginfo = (
        '    {"\\x61\\x62\\x63",\n     3,\n     "Test Description",\n'
        '     "Test Operator",\n     nullptr, 0}')
    self.assertEqual(
        make_ct_known_logs_list._to_loginfo_struct(log, 1), expected_loginfo)


  def testToLogInfoStructNoDNS(self):
    log = {
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
    }
    expected_loginfo = (
        '    {"\\x61\\x62\\x63",\n     3,\n     "Test Description",\n'
        '     "",\n     nullptr, 0}')
    self.assertEqual(
        make_ct_known_logs_list._to_loginfo_struct(log, 1), expected_loginfo)


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
        ',\n     "",\n     nullptr, 0},\n     '
        "base::Time::FromTimeT(1551083574)}")

    self.assertEqual(
        make_ct_known_logs_list._to_disqualified_loginfo_struct(log, 1),
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

class LogListTimestampGenerationTest(unittest.TestCase):

  def testGenerateLogListTimestamp(self):
    iso_timestamp = "2021-08-09T00:00:00Z"
    expected_generated_timestamp = (
        '// The time at which this log list was last updated.\n'
        'const base::Time kLogListTimestamp = '
        'base::Time::FromTimeT(1628467200);\n\n')

    self.assertEqual(
        make_ct_known_logs_list._generate_log_list_timestamp(iso_timestamp),
        expected_generated_timestamp)


class LogListPreviousOperatorsTest(unittest.TestCase):
  def testPreviousOperatorsStruct(self):
    log = {
        "log_id": "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=",
        "key": "YWJj",
        "description": "Test Description",
        "url": "ct.example.com",
        "previous_operators": [
          {"name": "test123",
           "end_time": "2021-01-01T00:00:00Z"
          },
          {"name": "test123456",
           "end_time": "2018-01-01T00:00:00Z"
          }
         ]
    }
    expected_previous_operator_info = (
        'const PreviousOperatorEntry kPreviousOperators1[] = {\n'
        '        {"test123456", base::Time::FromTimeT(1514764800)},\n'
        '        {"test123", base::Time::FromTimeT(1609459200)},};\n')
    self.assertEqual(
        make_ct_known_logs_list._to_previous_operators_struct(log, 1),
        expected_previous_operator_info)


if __name__ == "__main__":
  unittest.main()

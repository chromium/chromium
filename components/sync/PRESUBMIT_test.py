#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path
import sys
import unittest

import PRESUBMIT

sys.path.append(
    str(Path(__file__).resolve().parent / "../../third_party/depot_tools"))

from testing_support.presubmit_canned_checks_test_mocks import MockInputApi, MockOutputApi, MockAffectedFile


class CheckProtoVisitorChangeTest(unittest.TestCase):

    def test_no_warning(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile("components/sync/protocol/proto_visitors.h", ""),
            MockAffectedFile("components/sync/protocol/entity_specifics.proto",
                             ""),
        ]
        self.assertFalse(
            PRESUBMIT.CheckProtoVisitorChange(input_api, MockOutputApi()))

    def test_warning(self):
        input_api = MockInputApi()
        input_api.files = [
            MockAffectedFile("components/sync/protocol/entity_specifics.proto",
                             ""),
        ]
        self.assertTrue(
            PRESUBMIT.CheckProtoVisitorChange(input_api, MockOutputApi()))


if __name__ == "__main__":
    unittest.main()

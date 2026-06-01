#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import unittest
import unittest.mock

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.gn2bp.run_gn2bp as run_gn2bp  # pylint: disable=wrong-import-position
import components.cronet.tools.breakages_constants as breakages_constants  # pylint: disable=wrong-import-position


class TestRunGN2BPUnitTest(unittest.TestCase):

    def setUp(self):
        run_gn2bp.GetCurrentCheckoutBranchStableState.cache_clear()

    def test_bad_change_id_no_good_change_id_should_throw(self):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: there is a breakage that has not been fixed yet.',
            run_gn2bp.validate_release,
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: []
            }], {'foo': 0})

    def test_bad_change_id_and_no_good_change_id_but_but_both_not_in_history_should_throw(
            self):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: there is a breakage that has not been fixed yet.',
            run_gn2bp.validate_release,
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: []
            }], {})

    def test_bad_change_id_and_good_change_id_but_but_both_not_in_history_should_not_throw(
            self):
        run_gn2bp.validate_release(
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar']
            }], {})

    def test_bad_change_id_and_good_change_id_but_but_bad_not_in_history_should_not_throw(
            self):
        run_gn2bp.validate_release(
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar']
            }], {'bar': 0})

    def test_bad_change_id_and_good_change_id_but_not_in_history_should_throw(
            self):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: the current checkout includes a breaking change, but not its fix.',
            run_gn2bp.validate_release,
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar']
            }], {'foo': 0})

    def test_sort_versions(self):
        self.assertEqual(
            run_gn2bp.sort_versions(
                ["123.0.1000.0", "123.0.999.0", "123.0.1001.0"]),
            ["123.0.999.0", "123.0.1000.0", "123.0.1001.0"])

    def test_bad_change_id_and_good_change_id_in_history_should_not_throw(
            self):
        run_gn2bp.validate_release(
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar']
            }], {
                'bar': 0,
                'foo': 1
            })

    def test_bad_change_id_and_good_change_id_but_before_bad_change_id_should_throw(
            self):
        self.assertRaisesRegex(
            RuntimeError,
            'the local history shows a bad change ID that is more recent than its fix',
            run_gn2bp.validate_release,
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar']
            }], {
                'bar': 1,
                'foo': 0
            })

    def test_bad_change_id_but_two_good_change_ids_should_throw(self):
        self.assertRaisesRegex(
            RuntimeError,
            'Multiple good change IDs are only necessary when a fix has to be cherry-picked into a release',
            run_gn2bp.validate_release,
            [{
                breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar', 'bar_branch']
            }], {
                'bar': 0,
                'bar_branch': 1,
                'foo': 2
            })

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string')
    def test_GetCurrentCheckoutBranchStableState_stable(
            self, mock_latest_stable, mock_current):
        mock_current.return_value = "123.0.6300.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(run_gn2bp.GetCurrentCheckoutBranchStableState(),
                         run_gn2bp.BranchStableState.BRANCH_IS_STABLE)

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string')
    def test_GetCurrentCheckoutBranchStableState_old_stable(
            self, mock_latest_stable, mock_current):
        mock_current.return_value = "123.0.6200.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(run_gn2bp.GetCurrentCheckoutBranchStableState(),
                         run_gn2bp.BranchStableState.BRANCH_IS_OLD_STABLE)

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string')
    def test_GetCurrentCheckoutBranchStableState_not_yet_stable(
            self, mock_latest_stable, mock_current):
        mock_current.return_value = "123.0.6400.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(run_gn2bp.GetCurrentCheckoutBranchStableState(),
                         run_gn2bp.BranchStableState.BRANCH_IS_NOT_YET_STABLE)

    def test_get_build_number_from_version_string(self):
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string("123.0.6300.0"),
            6300)
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string("1.2.3.4"), 3)
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string(
                "100.200.30000.400"), 30000)


if __name__ == '__main__':
    # This allows you to run the file directly
    unittest.main()

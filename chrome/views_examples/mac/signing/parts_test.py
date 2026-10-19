# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import unittest
from unittest import mock

from signing import parts
from signing.model import CodeSignOptions, VerifyOptions


class TestGetParts(unittest.TestCase):
    def test_get_parts(self):
        config = mock.Mock()
        config.app_product = 'ViewsExamples'
        config.framework_dir = (
            'ViewsExamples.app/Contents/Frameworks/'
            'ViewsExamplesFramework.framework'
        )
        config.base_bundle_id = 'org.chromium.views_examples'
        config.codesign_requirements_outer_app = 'requirement'

        all_parts = parts.get_parts(config)
        self.assertEqual(3, len(all_parts))

        self.assertEqual(
            (
                'ViewsExamples.app/Contents/Frameworks/'
                'ViewsExamplesFramework.framework/Versions/Current/Libraries/'
                'libtest_trace_processor.dylib'
            ),
            all_parts[0].path,
        )
        self.assertEqual('libtest_trace_processor', all_parts[0].identifier)

        self.assertEqual(
            (
                'ViewsExamples.app/Contents/Frameworks/'
                'ViewsExamplesFramework.framework'
            ),
            all_parts[1].path,
        )
        self.assertEqual(
            'org.chromium.views_examples.framework', all_parts[1].identifier
        )

        self.assertEqual('ViewsExamples.app', all_parts[2].path)
        self.assertEqual('org.chromium.views_examples', all_parts[2].identifier)

        for part in all_parts[:2]:
            self.assertEqual(
                VerifyOptions.DEEP | VerifyOptions.STRICT, part.verify_options
            )

        self.assertEqual(
            CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS, all_parts[2].options
        )
        self.assertEqual('requirement', all_parts[2].requirements)

    @mock.patch.multiple(
        'signing.signing',
        **{
            m: mock.DEFAULT
            for m in ('sign_part', 'verify_part', 'validate_app')
        },
    )
    def test_sign_all(self, **kwargs):
        config = mock.Mock()
        config.app_product = 'ViewsExamples'
        config.framework_dir = (
            'ViewsExamples.app/Contents/Frameworks/'
            'ViewsExamplesFramework.framework'
        )
        config.base_bundle_id = 'org.chromium.views_examples'
        config.codesign_requirements_outer_app = 'requirement'
        paths = mock.Mock()

        asyncio.run(parts.sign_all(paths, config))

        self.assertEqual(3, kwargs['sign_part'].call_count)
        self.assertEqual(3, kwargs['verify_part'].call_count)
        kwargs['validate_app'].assert_awaited_once_with(paths, config, mock.ANY)

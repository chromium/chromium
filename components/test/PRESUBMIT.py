# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //components/test
See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'

def CheckChange(input_api, output_api):
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/custom_handlers/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/dom_distiller/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/feed/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/fenced_frames/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/history/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/history_embeddings/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/language/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/offline_pages/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/paint_preview/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/password_manager/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/performance_manager/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api, 'data/history/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/safe_browsing/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/service_worker/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/subresource_filter/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/url_rewrite/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/value_store/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/viz/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/webapps/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/web_database/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/webcrypto/unit_tests_bundle_data')
        results += presubmit_support.CheckBundleData(
                input_api, output_api,
                'data/web_package/unit_tests_bundle_data')
    finally:
        sys.path = old_sys_path
    return results

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def _CommonChecks(input_api, output_api):
    old_path = input_api.sys.path[:]
    try:
        tools_path = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                            'chrome', 'browser', 'glic',
                                            'tools')
        input_api.sys.path.insert(0, tools_path)
        import check_glic_api_test_registration
        return check_glic_api_test_registration.CheckGlicApiTestRegistration(
            input_api, output_api)
    finally:
        input_api.sys.path = old_path


def CheckChange(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)

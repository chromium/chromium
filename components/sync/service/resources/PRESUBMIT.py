# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  results = []
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results

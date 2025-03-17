# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

def CheckCommon(input_api, output_api):
  results = []
  results += input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api)
  return results

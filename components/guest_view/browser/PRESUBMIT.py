# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script to check that BadMessage enums in histograms.xml
match the corresponding bad_message.h file.
"""

def _RunHistogramChecks(input_api, output_api, histogram_name):
  try:
    # Setup sys.path so that we can call histograms code.
    import sys
    original_sys_path = sys.path
    sys.path = sys.path + [input_api.os_path.join(
      input_api.change.RepositoryRoot(),
      'tools', 'metrics', 'histograms')]

    import presubmit_bad_message_reasons
    return presubmit_bad_message_reasons.PrecheckBadMessage(input_api,
      output_api, histogram_name)
  except:
    return [output_api.PresubmitError('Could not verify histogram!')]
  finally:
    sys.path = original_sys_path

def CheckChangeOnUpload(input_api, output_api):
  return _RunHistogramChecks(input_api, output_api, "BadMessageReasonGuestView")

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
  results = []

  # Warn if the proto file is not modified without also modifying
  # the WebUI and extension API idl file.
  proto_path = 'components/safe_browsing/core/common/proto/csd.proto'
  realtime_proto_path = 'components/safe_browsing/core/common/proto/realtimeapi.proto'
  web_ui_path = 'components/safe_browsing/content/browser/web_ui/safe_browsing_ui.cc'
  idl_path = 'chrome/common/extensions/api/safe_browsing_private.idl'
  safebrowsingv5_proto_path = 'components/safe_browsing/core/common/proto/safebrowsingv5.proto'

  if proto_path in input_api.change.LocalPaths():
    if web_ui_path not in input_api.change.LocalPaths():
      results.append(
          output_api.PresubmitPromptWarning(
              'You modified the one or more of the CSD protos in: \n'
              '  ' + proto_path + '\n'
              'without changing the WebUI in: \n'
              '  ' + web_ui_path + '\n')
      )
    if idl_path not in input_api.change.LocalPaths():
      results.append(
          output_api.PresubmitPromptWarning(
              'You modified the one or more of the CSD protos in: \n'
              '  ' + proto_path + '\n'
              'without changing the API definition in: \n'
              '  ' + idl_path + '\n')
      )
    results.append(
          output_api.PresubmitPromptWarning(
              'You modified the one or more of the CSD protos in: \n'
              '  ' + proto_path + '\n'
              'Please remember to update the proto file in the internal ' +
              'repository. \n')
      )
  if realtime_proto_path in input_api.change.LocalPaths():
    results.append(
      output_api.PresubmitPromptWarning(
              'You modified the realtimeapi protos in: \n'
              '  ' + realtime_proto_path + '\n'
              'Please remember to update the proto file in the internal ' +
              'repository. \n')
    )
  if safebrowsingv5_proto_path in input_api.change.LocalPaths():
    results.append(
      output_api.PresubmitPromptWarning(
              'You modified the safebrowsingv5 protos in: \n'
              '  ' + safebrowsingv5_proto_path + '\n'
              'Please remember to update the proto file in the internal ' +
              'repository. \n')
    )
  return results

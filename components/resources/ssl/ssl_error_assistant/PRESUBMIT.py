# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks for ssl_error_assistant.asciipb.

   This is taken from chrome/browser/resources/safe_browsing/PRESUBMIT.py.
"""

# TODO(meacer): Refactor and reuse shared code with
#               chrome/browser/resources/safe_browsing/PRESUBMIT.py
def CheckVersionUpdatedInSSLErrorAssistantProto(input_api, output_api):
  # Don't report errors for "git cl presubmit --all/--files"
  if input_api.no_diffs:
    return []

  def IsSSLErrorAssistantProto(x):
    return (input_api.os_path.basename(x.LocalPath()) ==
            'ssl_error_assistant.asciipb')

  ssl_error_assistant_proto = input_api.AffectedFiles(
      file_filter=IsSSLErrorAssistantProto)
  if not ssl_error_assistant_proto:
    return []

  for _, line in ssl_error_assistant_proto[0].ChangedContents():
    if line.strip().startswith('version_id: '):
      return []

  # It's enticing to do something fancy like checking whether the ID was in fact
  # incremented or whether this is a whitespace-only or comment-only change.
  # However, currently deleted lines don't show up in ChangedContents() and
  # attempting to parse the asciipb file any more than we are doing above is
  # likely not worth the trouble.
  #
  # At worst, the submitter can skip the presubmit check on upload if it isn't
  # correct.
  return [output_api.PresubmitError(
      'Increment |version_id| in ssl_error_assistant.asciipb if you are '
      'updating the SSL Error Assistant proto.')]

def CheckChangeOnUpload(input_api, output_api):
  # TODO(asanka): Add a PRESUBMIT check for verifying that the
  # ssl_error_assistant.asciipb file is valid.
  return CheckVersionUpdatedInSSLErrorAssistantProto(input_api, output_api)

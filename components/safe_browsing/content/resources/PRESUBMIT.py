# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks for SafeBrowsing download_file_types.
"""

USE_PYTHON3 = True

def CheckVersionUpdatedInDownloadFileTypeList(input_api, output_api):
  def IsDownloadFileTypeList(x):
    return (input_api.os_path.basename(x.LocalPath()) ==
            'download_file_types.asciipb')

  download_file_types = input_api.AffectedFiles(
      file_filter=IsDownloadFileTypeList)
  if not download_file_types:
    return []

  for _, line in download_file_types[0].ChangedContents():
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
      'Increment |version_id| in download_file_types.asciipb if you are '
      'updating the file types proto.')]

def CheckChangeOnUpload(input_api, output_api):
  # TODO(asanka): Add a PRESUBMIT check for verifying that the
  # download_file_types.asciipb file is valid.
  return CheckVersionUpdatedInDownloadFileTypeList(input_api, output_api)

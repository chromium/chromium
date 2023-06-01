# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit checks for SafeBrowsing real_time_url_checks_allowlist.
"""

def _CheckIdHelper(contents, id_type):
    """Return the version or scheme id from the
    provided file contents.
    """
    for line in contents:
        line_split = line.split(id_type + ': ')
        if len(line_split) > 1:
            return line_split[1]
    return -1

def CheckVersionUpdatedInRealTimeUrlChecksAllowlist(
    output_api, allowlist_ascii_file
):
    # Report error if version id was not changed
    results = []

    old_version_id = int(
        _CheckIdHelper(allowlist_ascii_file.OldContents(), 'version_id')
    )
    new_version_id = int(
        _CheckIdHelper(allowlist_ascii_file.NewContents(), 'version_id')
    )

    # Check old version id against new id
    if new_version_id != old_version_id + 1:
        results.append(
            output_api.PresubmitError(
                'The new |version_id| in ' +
                allowlist_ascii_file.LocalPath() + ' should be '
                'the incremented old |version_id| if you are updating the real '
                'time url allowlist proto.'))
    return results

def CheckSchemeUpdatedInRealTimeUrlChecksAllowlist(
    output_api, allowlist_ascii_file
):
  # Report error if scheme id is invalid
    results = []

    old_scheme_id = int(
        _CheckIdHelper(allowlist_ascii_file.OldContents(), 'scheme_id')
    )
    new_scheme_id = int(
        _CheckIdHelper(allowlist_ascii_file.NewContents(), 'scheme_id')
    )

    # Check old scheme id against new id
    if new_scheme_id < old_scheme_id or new_scheme_id > old_scheme_id + 1:
        results.append(
            output_api.PresubmitError(
                'The new |scheme_id| in ' +
                allowlist_ascii_file.LocalPath() + ' should be '
                'the same or the incremented old |scheme_id| if you are '
                'updating the real time url allowlist proto.'
            )
        )
    results.append(
        output_api.PresubmitPromptWarning(
            'In most scenarios, the scheme_id should stay the same. However, '
            'when there are changes made to the format of the proto file, the '
            'scheme_id should be incremented by 1. If this happens, there will '
            'also be changes to the implementation of the real time URL checks '
            'alowlist. If this is not happening, please do not change the '
            'scheme_id.'
        )
    )
    return results

def CheckChangeOnUpload(input_api, output_api):
  # If there are no changes, return no warnings
  if input_api.no_diffs:
    return []

  def IsRealTimeUrlAllowlistAsciiPb(x):
    return input_api.os_path.basename(
      x.LocalPath()) == 'real_time_url_allowlist.asciipb'

  real_time_url_allowlist_ascii_file = input_api.AffectedFiles(
    file_filter=IsRealTimeUrlAllowlistAsciiPb)
  if not real_time_url_allowlist_ascii_file:
    return []

  ascii_file = real_time_url_allowlist_ascii_file[0]
  if len(ascii_file.NewContents()) != 3:
    return [
        output_api.PresubmitError(
            'The asciipb file located at ' +
            allowlist_ascii_file.LocalPath() + ' should have '
            'three fields: version_id, scheme_id, and url_hashes, ' +
            'each on its own line within the file.'
        )
    ]

  # If the asciipb file is being added in this CL, then we will not
  # check it against the old contents.
  if len(ascii_file.OldContents()) == 0:
    return []

  # Check the new version of the component with the old to ensure validity
  return (
    CheckVersionUpdatedInRealTimeUrlChecksAllowlist(
        output_api, ascii_file
    ) +
    CheckSchemeUpdatedInRealTimeUrlChecksAllowlist(
        output_api, ascii_file
    )
  )

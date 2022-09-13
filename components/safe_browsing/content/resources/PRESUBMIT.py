# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks for SafeBrowsing download_file_types.
"""

USE_PYTHON3 = True

def CheckVersionUpdatedInDownloadFileTypeList(input_api, output_api):
    # Don't report errors for "git cl presubmit --all/--files"
    if input_api.no_diffs:
        return []

    download_file_type_names = [
        'download_file_types.asciipb', 'download_file_types_experiment.asciipb'
    ]

    def IsDownloadFileTypeList(x):
        return input_api.os_path.basename(
            x.LocalPath()) in download_file_type_names

    download_file_types_files = input_api.AffectedFiles(
        file_filter=IsDownloadFileTypeList)
    if not download_file_types_files:
        return []

    results = []
    for download_file_types_file in download_file_types_files:
        has_changed_version = False
        for _, line in download_file_types_file.ChangedContents():
            if line.strip().startswith('version_id: '):
                has_changed_version = True
                break

        # It's enticing to do something fancy like checking whether the ID was
        # in fact incremented or whether this is a whitespace-only or
        # comment-only change. However, currently deleted lines don't show up in
        # ChangedContents() and attempting to parse the asciipb file any more
        # than we are doing above is likely not worth the trouble.
        #
        # At worst, the submitter can skip the presubmit check on upload if it
        # isn't correct.
        if not has_changed_version:
            results.append(
                output_api.PresubmitError(
                    'Increment |version_id| in ' +
                    download_file_types_file.LocalPath() + ' if you are '
                    'updating the file types proto.'))

    if len(download_file_types_files) == 1:
        results.append(
            output_api.PresubmitPromptWarning(
                'You only modified either of download_file_types.asciipb or ' +
                'download_file_types_experiment.asciipb, please make sure ' +
                'your change does not affect any ongoing experiment.'))

    results.append(
        output_api.PresubmitPromptWarning(
            'Please make sure you have read https://chromium.googlesource.com'
            + '/chromium/src/+/HEAD/chrome/browser/resources/safe_browsing/' +
            'README.md before editing the download_file_types config files.'))

    return results


def CheckChangeOnUpload(input_api, output_api):
    # TODO(asanka): Add a PRESUBMIT check for verifying that the
    # download_file_types.asciipb file is valid.
    return CheckVersionUpdatedInDownloadFileTypeList(input_api, output_api)

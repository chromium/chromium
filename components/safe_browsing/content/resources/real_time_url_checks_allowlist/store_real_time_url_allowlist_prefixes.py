#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
 Fetch the real time URL allowlist hash prefixes using the SBv5 API (gca-32b).
 Once we've validated the response, save these concatenated prefixes as the
 url_hashes in the real_time_url_allowlist.asciipb file.
"""

import optparse
import os
import subprocess
import sys
import traceback
import urllib.parse
import urllib.request

from validation_utils import (
    HASH_PREFIX_SIZE,
    CheckHashPrefixesAreValid,
    CheckHashPrefixesHaveChanged,
)

sys.path.append(
    os.path.abspath(
        os.path.join(os.path.abspath(__file__),
                     *[os.path.pardir] * 6 + ['google_apis'])))
import google_api_keys


def FetchSafeBrowsingAllowlistHashes(buildpath):
    """Return a byte sequence of concatenated 16 byte URL hash prefixes,
    fetched from the Safe Browsing API v5 endpoint (gca-32b list).
    """
    # Fetch the serialized V5 BatchGetHashListsResponse proto for the gca-32b
    # list.
    url = 'https://safebrowsing.googleapis.com/v5/hashLists:batchGet?'
    params_dict = {
        'names': 'gca-32b',
        'key': google_api_keys.GetAPIKey(),
    }

    params = urllib.parse.urlencode(params_dict)
    req = urllib.request.Request(url + params)

    response_proto_bytes = urllib.request.urlopen(req).read()
    assert (len(response_proto_bytes)
            > 0), 'Bad response from SB API - empty response received'

    assert sys.platform.startswith('linux'), (
        'Updating the real-time URL allowlist is only supported on Linux since '
        'Android builds are only supported on Linux. If this ever changes, '
        'autoninja/executable path resolution below and binary stdin/stdout '
        'handling in v5_rice_decoder_tool will need to be re-evaluated for '
        'other platforms.')

    # Build the host C++ tool that decodes 256-bit Rice-encoded full hashes and
    # truncates each 32-byte full hash to a 16-byte prefix.
    target = ('components/safe_browsing/content/resources/'
              'real_time_url_checks_allowlist:v5_rice_decoder_tool')
    subprocess.run(['autoninja', '-C', buildpath, target],
                   cwd=buildpath,
                   check=True)

    # Locate the compiled host binary (under clang_x64/ or clang_arm64/ for
    # cross-compiled Android build directories, or directly in buildpath for host
    # build directories), picking the most recently modified binary if multiple
    # exist.
    candidate_paths = [
        os.path.join(buildpath, 'clang_x64', 'v5_rice_decoder_tool'),
        os.path.join(buildpath, 'clang_arm64', 'v5_rice_decoder_tool'),
        os.path.join(buildpath, 'v5_rice_decoder_tool'),
    ]
    existing_paths = [p for p in candidate_paths if os.path.exists(p)]
    assert existing_paths, (
        f'Failed to find compiled v5_rice_decoder_tool in {buildpath}')
    tool_bin = max(existing_paths, key=os.path.getmtime)

    # Pipe the raw protobuf response into the decoder tool and read the
    # concatenated 16-byte hash prefixes from stdout.
    proc = subprocess.run(
        [tool_bin],
        input=response_proto_bytes,
        capture_output=True,
    )
    if proc.returncode != 0:
        stderr_msg = proc.stderr.decode('utf-8', errors='replace').strip()
        raise RuntimeError(
            f'v5_rice_decoder_tool failed (exit {proc.returncode}): {stderr_msg}'
        )
    return proc.stdout


def WriteHashesToFile(hash_prefixes):
    """Write provided hashes to the real_time_url_allowlist.asciipb file"""
    outfile = os.path.join(os.getcwd(), 'real_time_url_allowlist.asciipb')
    # Read the ASCII
    with open(outfile, 'r') as ifile:
        ascii_pb_str = ifile.read()
    # New contents should keep version_id and scheme_id then replace url_hashes
    new_contents = (ascii_pb_str.split('url_hashes')[0] + 'url_hashes: ' +
                    str(hash_prefixes)[1:])
    # Write new ASCII contents
    with open(outfile, 'w') as ofile:
        ofile.write(new_contents)


class StoreRealTimeUrlAllowlistPrefixes:

    def Run(self):
        parser = optparse.OptionParser()
        parser.add_option(
            '-p',
            '--buildpath',
            help='File path of the out build directory. Required for compiling'
            ' and running the v5 Rice decoder tool.')

        (opts, args) = parser.parse_args()
        if opts.buildpath is None:
            parser.print_help()
            return 1

        buildpath = os.path.abspath(os.path.expanduser(opts.buildpath))

        try:
            hash_prefixes = FetchSafeBrowsingAllowlistHashes(buildpath)
            CheckHashPrefixesAreValid(hash_prefixes)
            assert CheckHashPrefixesHaveChanged(hash_prefixes), (
                "The URL hash prefixes have not changed, so we won't update the file"
            )
            WriteHashesToFile(hash_prefixes)
            num_prefixes = len(hash_prefixes) // HASH_PREFIX_SIZE
            print(
                f'Successfully validated and wrote {num_prefixes} URL hash prefixes'
                ' to real_time_url_allowlist.asciipb.')
        except Exception as e:
            print(
                "ERROR: Failed to receive valid response from SB API:\n  %s\n%s"
                % (str(e), traceback.format_exc()))
            return 1


def main():
    return StoreRealTimeUrlAllowlistPrefixes().Run()


if __name__ == '__main__':
    sys.exit(main())

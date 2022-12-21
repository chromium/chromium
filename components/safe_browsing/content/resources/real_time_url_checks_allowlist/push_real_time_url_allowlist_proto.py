#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build and push the {vers}/android/real_time_url_checks_allowlist.pb file to
# GCS so that the component update system will pick them up and push them to
# users.  See README.md before running this.
#
# Requires ninja and gsutil to be in the user's path.

import optparse
import os
import shutil
import subprocess
import sys


DEST_BUCKET = 'gs://chrome-component-real-time-url-checks-allowlist'
RESOURCE_SUBDIR = (
    'components/safe_browsing/content/resources/real_time_url_checks_allowlist'
)


def main():
    parser = optparse.OptionParser()
    parser.add_option('-d',
                      '--dir',
                      help='An up-to-date GN/Ninja build directory, '
                      'such as ./out/Debug')

    (opts, args) = parser.parse_args()
    if opts.dir is None:
        parser.print_help()
        return 1

    # Clear out the target dir before we build so we can be sure we've got
    # the freshest version.
    allowlist_dir = os.path.join(opts.dir, "gen", RESOURCE_SUBDIR, 'allowlist')
    if os.path.isdir(allowlist_dir):
        shutil.rmtree(allowlist_dir)

    script_name = ':make_real_time_url_allowlist_protobuf_for_gcs'
    gn_command = ['ninja', '-C', opts.dir, RESOURCE_SUBDIR + script_name]
    print("Running the following")
    print("   " + (' '.join(gn_command)))
    if subprocess.call(gn_command):
        print("Ninja failed.")
        return 1

    os.chdir(allowlist_dir)

    # Sanity check that we're in the right place
    dirs = os.listdir('.')
    assert len(dirs) == 1 and dirs[0].isdigit(), (
        "Confused by lack of single versioned dir under " + allowlist_dir)

    # Push the file with its directories, in the form
    #   {vers}/android/real_time_url_checks_allowlist.pb
    # Don't overwrite existing files, in case we forgot to increment the
    # version.
    vers_dir = dirs[0]
    command = ['gsutil', 'cp', '-Rn', vers_dir, DEST_BUCKET]

    print('\nGoing to run the following command')
    print('   ', ' '.join(command))
    print('\nIn directory')
    print('   ', allowlist_dir)
    print('\nWhich should push the following files')
    expected_files = [
        os.path.join(dp, f) for dp, dn, fn in os.walk(vers_dir) for f in fn
    ]
    for f in expected_files:
        print('   ', f)

    shall = input('\nAre you sure (y/N) ').lower() == 'y'
    if not shall:
        print('aborting')
        return 1
    return subprocess.call(command)


if __name__ == '__main__':
    sys.exit(main())

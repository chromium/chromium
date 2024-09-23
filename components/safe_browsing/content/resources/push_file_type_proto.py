#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build and push the {vers}/{platform}/download_file_types.pb files to GCS so
# that the component update system will pick them up and push them
# to users.  See README.md before running this.
#
# Requires ninja and gsutil to be in the user's path.

import optparse
import os
import shutil
import subprocess
import sys

DEST_BUCKET = 'gs://chrome-component-file-type-policies'
RESOURCE_SUBDIR = 'components/safe_browsing/content/resources'


def GetConfigVersion(ascii_pb_path):
    # Parsing the proto requires integration into the build system to
    # generate the py bindings for DownloadFileTypeConfig. But this
    # script intentionally has minimal interaction with the build system
    # since builders start by building all gn targets. Instead, we
    # simply look for the version_id in the text format.
    with open(ascii_pb_path) as ascii_pb_file:
      for line in ascii_pb_file:
          split = line.split("version_id: ")
          if len(split) == 2:
              return int(split[1])
    return None


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

    version_id = GetConfigVersion(os.path.join(RESOURCE_SUBDIR,
                                               'download_file_types.asciipb'))
    assert version_id, "Failed to get version_id from generated config pb"

    # Clear out the target dir before we build so we can be sure we've got
    # the freshest version.
    all_dir = os.path.join(opts.dir, "gen", RESOURCE_SUBDIR, 'all')
    if os.path.isdir(all_dir):
        shutil.rmtree(all_dir)

    gn_command = [
        'autoninja', '-C', opts.dir,
        RESOURCE_SUBDIR + ':make_all_file_types_protobuf'
    ]
    print("Running the following")
    print("   " + (' '.join(gn_command)))
    if subprocess.call(gn_command):
        print("Autoninja failed.")
        return 1

    os.chdir(all_dir)

    # Sanity check that we're in the right place
    dirs = os.listdir('.')
    assert sorted(dirs) == ['android', 'chromeos', 'linux', 'mac', 'win'], (
        "Confused by wrong platform dirs under " + all_dir)

    # Push the files with their directories, in the form
    #   {vers}/{platform}/download_file_types.pb
    # Don't overwrite existing files, in case we forgot to increment the
    # version.
    command = ['gsutil', 'cp', '-Rn', '.', DEST_BUCKET + "/" + str(version_id)]

    print('\nGoing to run the following command')
    print('   ', ' '.join(command))
    print('\nIn directory')
    print('   ', all_dir)
    print('\nWhich should push the following files')
    expected_files = [
        os.path.join(dp, f) for dp, dn, fn in os.walk('.') for f in fn
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

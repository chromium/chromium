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


def main():
    parser = optparse.OptionParser()
    parser.add_option('-d',
                      '--dir',
                      help='An up-to-date GN/Ninja build directory, '
                      'such as ./out/Debug')
    parser.add_option('--experiment',
                      action='store_true',
                      default=False,
                      help='When this flag is set to true, '
                      'push the experiment version of file type policies.'
                      'This version is defined in '
                      'download_file_types_experiment.asciipb')

    (opts, args) = parser.parse_args()
    if opts.dir is None:
        parser.print_help()
        return 1

    # Clear out the target dir before we build so we can be sure we've got
    # the freshest version.
    all_sub_dir = 'experiment_all' if opts.experiment else 'all'
    all_dir = os.path.join(opts.dir, "gen", RESOURCE_SUBDIR, all_sub_dir)
    if os.path.isdir(all_dir):
        shutil.rmtree(all_dir)

    script_name = ':make_all_file_types_protobuf_experiment' if \
      opts.experiment else ':make_all_file_types_protobuf'
    gn_command = ['ninja', '-C', opts.dir, RESOURCE_SUBDIR + script_name]
    print("Running the following")
    print("   " + (' '.join(gn_command)))
    if subprocess.call(gn_command):
        print("Ninja failed.")
        return 1

    os.chdir(all_dir)

    # Sanity check that we're in the right place
    dirs = os.listdir('.')
    assert len(dirs) == 1 and dirs[0].isdigit(), (
        "Confused by lack of single versioned dir under " + all_dir)

    # Push the files with their directories, in the form
    #   {vers}/{platform}/download_file_types.pb
    # Don't overwrite existing files, in case we forgot to increment the
    # version.
    vers_dir = dirs[0]
    command = ['gsutil', 'cp', '-Rn', vers_dir, DEST_BUCKET]

    print('\nGoing to run the following command')
    print('   ', ' '.join(command))
    print('\nIn directory')
    print('   ', all_dir)
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

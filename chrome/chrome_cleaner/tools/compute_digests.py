# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Computes digests for files, and stores them in a proto file.

This script's input is a file that contains a newline-separated list of files
for which digests are to be computed. Once computed, the digests are put in a
FileDigests protobuf message, which is saved in binary format to the specified
file.
"""

import argparse
import hashlib
import import_util
import os
import sys


# Assume we are running from the build dir where protos are generated.
import_util.AddProtosToPath(os.getcwd())
import file_digest_pb2


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('file', type=str, nargs='+',
                      help='paths to the files for which digests need to be '
                           'computed')
  parser.add_argument('--output', type=str, required=True,
                      help='path to the file where the proto with the ' +
                           'computed digests should be saved')
  args = parser.parse_args()

  # Compute each file's digest, and store it in |proto|.
  proto = file_digest_pb2.FileDigests()
  for current_file in args.file:
    digest = hashlib.sha256()
    with open(current_file, 'rb') as infile:
      digest.update(infile.read())
    file_digest = proto.file_digests.add()
    file_digest.filename = os.path.basename(current_file)
    file_digest.digest = digest.hexdigest()

  # Write |proto| to |args.output|.
  with open(args.output, 'wb') as outfile:
    outfile.write(proto.SerializeToString())

  return 0


if __name__ == '__main__':
  sys.exit(main())

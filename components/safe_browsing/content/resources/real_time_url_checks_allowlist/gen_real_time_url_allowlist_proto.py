#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
 Convert the ASCII real_time_url_allowlist.asciipb proto into a binary resource.
"""

import os
import re
import sys

from validation_utils import CheckHashPrefixesAreValid

# Import the binary proto generator. Walks up to the root of the source tree
# which is five directories above, and the finds the protobufs directory from
# there.
proto_generator_path = os.path.normpath(
    os.path.join(os.path.abspath(__file__),
                 *[os.path.pardir] * 6 + ['components/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator

def WritePbFile(full_pb, outfile):
    """ Write the allowlist protobuf to |outfile| """
    # Serialize pb
    binary_pb_str = full_pb.SerializeToString()
    # Write pb to disk
    open(outfile, 'wb').write(binary_pb_str)

def MakeSubDirs(outfile):
    """ Make the subdirectories needed to create file |outfile| """
    dirname = os.path.dirname(outfile)
    if not os.path.exists(dirname):
        os.makedirs(dirname)


class RealTimeUrlAllowlistProtoGenerator(BinaryProtoGenerator):
    def ImportProtoModule(self):
        import realtimeallowlist_pb2
        globals()['realtimeallowlist_pb2'] = realtimeallowlist_pb2

    def EmptyProtoInstance(self):
        return realtimeallowlist_pb2.HighConfidenceAllowlist()

    def ValidatePb(self, opts, pb):
        """ Validate the basic url_hashes value of the protobuf.  The
        real_time_url_checks_allowlist_resource_file_unittest.cc will also
        validate it, but this will catch errors earlier.
        """
        CheckHashPrefixesAreValid(pb.url_hashes)

    def ProcessPb(self, opts, pb):
        """ Generate a binary proto using the parsed proto. """
        outfile = os.path.join(opts.outdir, opts.outbasename)
        if opts.gcs:
            # File path should be
            # ../allowlist/{vers}/android/real_time_url_checks_allowlist.pb
            outfile = os.path.join(opts.outdir, str(pb.version_id),
                                   'android', opts.outbasename)
        MakeSubDirs(outfile)
        WritePbFile(pb, outfile)

    def AddCommandLineOptions(self, parser):
        parser.add_option('-g',
                          '--gcs',
                          action="store_true",
                          default=False,
                          help='Write the file to the GCS location.')

    def AddExtraCommandLineArgsForVirtualEnvRun(self, opts, command):
        if opts.gcs:
            command += ['-g']

def main():
    return RealTimeUrlAllowlistProtoGenerator().Run()


if __name__ == '__main__':
    sys.exit(main())

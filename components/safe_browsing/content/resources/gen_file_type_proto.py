#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
 Convert the ASCII download_file_types.asciipb proto into a binary resource.

 We generate a separate variant of the binary proto for each platform,
 each which contains only the values that platform needs.
"""

from __future__ import absolute_import
from __future__ import print_function
import os
import re
import sys

# Import the binary proto generator. Walks up to the root of the source tree
# which is five directories above, and the finds the protobufs directory from
# there.
proto_generator_path = os.path.normpath(
    os.path.join(os.path.abspath(__file__),
                 *[os.path.pardir] * 5 + ['components/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator


# Map of platforms for which we can generate binary protos.
# This must be run after the custom imports.
#   key: type-name
#   value: proto-platform_type (int)
def PlatformTypes():
    return {
        # LINT.IfChange(PlatformTypes)
        "android":
        download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_ANDROID,
        "chromeos":
        download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_CHROME_OS,
        "linux": download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_LINUX,
        "mac": download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_MAC,
        "win": download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_WINDOWS,
        # LINT.ThenChange(BUILD.gn:PlatformTypes)
    }


def PrunePlatformSettings(file_type, default_settings, platform_type):
    # Modify this file_type's platform_settings by keeping the only the
    # best one for this platform_type. In order of preference:
    #   * Exact match to platform_type
    #   * PLATFORM_TYPE_ANY entry
    #   * or copy from the default file type.

    last_platform = -1
    setting_match = None
    for s in file_type.platform_settings:
        # Enforce: sorted and no dups (signs of mistakes).
        assert last_platform < s.platform, (
            "Extension '%s' has duplicate or out of order platform: '%s'" %
            (file_type.extension, s.platform))
        last_platform = s.platform

        # Pick the most specific match.
        if ((s.platform == platform_type) or
            (s.platform == \
              download_file_types_pb2.DownloadFileType.PLATFORM_TYPE_ANY and \
             setting_match is None)):
            setting_match = s

    # If platform_settings was empty, we'll fill in from the default
    if setting_match is None:
        assert default_settings is not None, (
            "Missing default settings for platform %d" % platform_type)
        setting_match = default_settings

    # Now clear out the full list and replace it with 1 entry.
    del file_type.platform_settings[:]
    new_setting = file_type.platform_settings.add()
    new_setting.CopyFrom(setting_match)
    new_setting.ClearField('platform')


def FilterPbForPlatform(full_pb, platform_type):
    """ Return a filtered protobuf for this platform_type """
    assert type(platform_type) is int, "Bad platform_type type"

    new_pb = download_file_types_pb2.DownloadFileTypeConfig()
    new_pb.CopyFrom(full_pb)

    # Ensure there's only one platform_settings for the default.
    PrunePlatformSettings(new_pb.default_file_type, None, platform_type)

    # This can be extended if we want to match weird extensions.
    # Just no dots, non-UTF8, or uppercase chars.
    invalid_char_re = re.compile('[^a-z0-9_-]')

    # Filter platform_settings for each type.
    uma_values_used = set()
    extensions_used = set()
    for file_type in new_pb.file_types:
        assert not invalid_char_re.search(file_type.extension), (
            "File extension '%s' contains non alpha-num-dash chars" %
            (file_type.extension))
        assert file_type.extension not in extensions_used, (
            "Duplicate extension '%s'" % file_type.extension)
        extensions_used.add(file_type.extension)

        assert file_type.uma_value not in uma_values_used, (
            "Extension '%s' reused UMA value %d." %
            (file_type.extension, file_type.uma_value))
        uma_values_used.add(file_type.uma_value)

        # Modify file_type to include only the best match platform_setting.
        PrunePlatformSettings(
            file_type, new_pb.default_file_type.platform_settings[0], \
              platform_type)

    return new_pb


def FilterForPlatformAndWrite(full_pb, platform_type, outfile):
    """ Filter and write out a file for this platform """
    # Filter it
    filtered_pb = FilterPbForPlatform(full_pb, platform_type)
    # Serialize it
    binary_pb_str = filtered_pb.SerializeToString()
    # Write it to disk
    open(outfile, 'wb').write(binary_pb_str)


def MakeSubDirs(outfile):
    """ Make the subdirectories needed to create file |outfile| """
    dirname = os.path.dirname(outfile)
    if not os.path.exists(dirname):
        os.makedirs(dirname)


class DownloadFileTypeProtoGenerator(BinaryProtoGenerator):
    def ImportProtoModule(self):
        import download_file_types_pb2
        globals()['download_file_types_pb2'] = download_file_types_pb2

    def EmptyProtoInstance(self):
        return download_file_types_pb2.DownloadFileTypeConfig()

    def ValidatePb(self, opts, pb):
        """ Validate the basic values of the protobuf.  The
        file_type_policies_unittest.cc will also validate it by platform,
        but this will catch errors earlier.
    """
        assert pb.version_id > 0
        assert pb.sampled_ping_probability >= 0.0
        assert pb.sampled_ping_probability <= 1.0
        assert len(pb.default_file_type.platform_settings) >= 1
        assert len(pb.file_types) > 1

    def ProcessPb(self, opts, pb):
        """ Generate one or more binary protos using the parsed proto. """
        if opts.type is not None:
            # Just one platform type
            platform_enum = PlatformTypes()[opts.type]
            outfile = os.path.join(opts.outdir, opts.outbasename)
            FilterForPlatformAndWrite(pb, platform_enum, outfile)
        else:
            # Make a separate file for each platform
            for platform_type, platform_enum in PlatformTypes().items():
                # e.g. .../all/chromeos/download_file_types.pb
                outfile = os.path.join(opts.outdir,
                                       platform_type, opts.outbasename)
                MakeSubDirs(outfile)
                FilterForPlatformAndWrite(pb, platform_enum, outfile)

    def AddCommandLineOptions(self, parser):
        parser.add_option('-a',
                          '--all',
                          action="store_true",
                          default=False,
                          help='Write a separate file for every platform.')
        parser.add_option(
            '-t',
            '--type',
            help='The platform type. One of android, chromeos, ' +
            'linux, mac, win')

    def AddExtraCommandLineArgsForVirtualEnvRun(self, opts, command):
        if opts.type is not None:
            command += ['-t', opts.type]
        if opts.all:
            command += ['-a']

    def VerifyArgs(self, opts):
        if (not opts.all and opts.type not in PlatformTypes()):
            print("ERROR: Unknown platform type '%s'" % opts.type)
            self.opt_parser.print_help()
            return False

        if (bool(opts.all) == bool(opts.type)):
            print("ERROR: Need exactly one of --type or --all")
            self.opt_parser.print_help()
            return False
        return True


def main():
    return DownloadFileTypeProtoGenerator().Run()


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool for tagging an updater metainstaller.

For example:
python3 chrome/updater/tools/tag.py --certificate_tag=certificate_tag.exe
    --in_file=UpdaterSetup.signed.exe --out_file=ChromeSetup.exe
    --tag=appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}

The script requires the presence of `certificate_tag.exe` and a signed
metainstaller. If the --out_file argument is not specified, then the output
goes to a file named `tagged_<in_file>`.

The tag is encoded as an ASCII string.

To run locally:
1. find the certificate_tag.exe. This file is usually in the build out dir.
2. use the signing/sign.py script to sign the updatersetup.exe target.
3. run this script as suggested above, using the correct paths for the args.
"""

import argparse
import binascii
import os.path
import struct
import subprocess


class TaggingError(Exception):
    """Module exception class."""


class Tagger:
    """A container for a tagging operation."""

    def __init__(self, tagging_exe):
        """Inits a tagger with the certificate tag tool."""
        self._tagging_exe = tagging_exe

    def _make_hex_tag(self, tag):
        """ Builds the string which gets embedded in the metainstaller.

        The tag contains a magic start, followed by a 2-byte big endian value
        representing the length of the tag, followed by the tag bytes. The
        entire tag is hex-encoded."""
        if len(tag) > 0xFFFF:
            raise TaggingError('Tag is too long.')
        bin_tag = bytearray(binascii.hexlify('Gact2.0Omaha'.encode()))
        bin_tag.extend(binascii.hexlify(struct.pack(">H", len(tag))))
        bin_tag.extend(binascii.hexlify(tag.encode()))
        return bin_tag.decode()

    def _insert_tag(self, tag, in_file, out_file):
        """Inserts the tag. This overrides any tag previously present in
        the metainstaller."""
        subprocess.run([
            self._tagging_exe,
            '--set-superfluous-cert-tag=0x%s' % self._make_hex_tag(tag),
            '--padded-length=8206',
            '--out=%s' % out_file, in_file
        ],
                       check=True)

    def tag_metainstaller(self, tag, in_file, out_file):
        if not out_file:
            out_file = os.path.join(os.path.dirname(in_file),
                                    'tagged_' + os.path.basename(in_file))
        return self._insert_tag(tag, in_file, out_file)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--certificate_tag',
                        required=True,
                        help='The path to the certificate_tag executable.')
    parser.add_argument('--tag',
                        required=True,
                        help='The tag as an ASCII string.')
    parser.add_argument('--in_file',
                        required=True,
                        help='The path to the signed metainstaller.')
    parser.add_argument('--out_file',
                        required=False,
                        help='The path to save the tagged metainstaller to.'
                        ' "tagged_" is prepended to in_file name if'
                        ' the out_file is not specified.')

    args = parser.parse_args()
    Tagger(args.certificate_tag).tag_metainstaller(args.tag, args.in_file,
                                                   args.out_file)


if __name__ == '__main__':
    main()

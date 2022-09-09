#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool for signing an updater metainstaller and contained code.

For example:
python3 sign.py --in_file UpdaterSetup.exe --out_file UpdaterSetup.signed.exe

To run locally:
 1. Create a self-signed developer certificate if you haven't yet by executing
 `New-SelfSignedCertificate -DnsName id@domain.tld -Type CodeSigning
 -CertStoreLocation cert:\CurrentUser\My` in powershell.
 2. Run `autoninja -C .\out\yourBuildDir chrome/updater`.
 3. cd to your build dir and execute this script.
"""

import argparse
import os.path
import shutil
import subprocess
import tempfile

import resedit


class SigningError(Exception):
    """Module exception class."""


class Signer:
    """A container for a signing operation."""

    def __init__(self, tmpdir, lzma_exe, signtool_exe, tagging_exe, identity):
        """Inits a signer with the necessary tools."""
        self._tmpdir = tmpdir
        self._lzma_exe = lzma_exe
        self._signtool_exe = signtool_exe
        self._tagging_exe = tagging_exe
        self._identity = identity

    def _add_tagging_cert(self, in_file):
        """Adds the tagging cert. Returns the path to the tagged file."""
        out_file = os.path.join(tempfile.mkdtemp(dir=self._tmpdir),
                                'tagged_file')
        subprocess.run([
            self._tagging_exe, '--set-superfluous-cert-tag=Gact2.0Omaha',
            '--padded-length=8206',
            '--out=%s' % out_file, in_file
        ],
                       check=True)
        return out_file

    def _sign_item(self, in_file):
        """Sign an executable in-place."""
        # Retries may be required: lore states the timestamp server is flaky.
        command = [
            self._signtool_exe, 'sign', '/v', '/tr',
            'http://timestamp.digicert.com', '/td', 'SHA256', '/fd', 'SHA256',
            '/s', 'my', '/n', self._identity, in_file
        ]
        subprocess.run(command, check=True)

    def _sign_7z(self, in_file):
        """Extract, sign, and rearchive the contents of a 7z archive."""
        tmp = tempfile.mkdtemp(dir=self._tmpdir)
        subprocess.run([self._lzma_exe, 'x', in_file,
                        '-o%s' % tmp],
                       check=True)
        signable_exts = frozenset([
            '.exe', '.dll', '.msi', '.cat', '.ps1', '.psm1', '.psd1', '.ps1xml'
        ])
        for root, _, files in os.walk(tmp):
            for f in files:
                ext = os.path.splitext(f)[1].lower()
                if ext in signable_exts:
                    self._sign_item(os.path.join(root, f))
                elif ext == '.7z':
                    self._sign_7z(os.path.join(root, f))
        subprocess.run([self._lzma_exe, 'a', '-mx0', in_file, '*'],
                       check=True,
                       cwd=tmp)

    def sign_metainstaller(self, in_file):
        """Return a path to a signed copy of an updater metainstaller."""
        workdir = tempfile.mkdtemp(dir=self._tmpdir)
        out_metainstaller = os.path.join(workdir, "metainstaller.exe")
        resed = resedit.ResourceEditor(in_file, out_metainstaller)
        resource = 'updater.packed.7z'
        extracted_7z = os.path.join(workdir, resource)
        resed.ExtractResource('B7', 1033, resource, extracted_7z)
        self._sign_7z(extracted_7z)
        resed.UpdateResource('B7', 1033, resource, extracted_7z)
        resed.Commit()
        self._sign_item(out_metainstaller)
        return self._add_tagging_cert(out_metainstaller)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--in_file',
                        required=True,
                        help='The path to the metainstaller.')
    parser.add_argument('--out_file',
                        required=True,
                        help='The path to save the signed metainstaller to.')
    parser.add_argument('--lzma_7z',
                        default='7z.exe',
                        help='The path to the 7z executable.')
    parser.add_argument(
        '--signtool',
        default='signtool.exe',
        help='The path to the signtool executable. Look in depot_tools.')
    parser.add_argument('--certificate_tag',
                        default='.\certificate_tag.exe',
                        help='The path to the certificate_tag executable.')
    parser.add_argument('--identity',
                        default='Google',
                        help='The signing identity to use.')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmpdir:
        shutil.move(
            Signer(tmpdir, args.lzma_7z, args.signtool, args.certificate_tag,
                   args.identity).sign_metainstaller(args.in_file),
            args.out_file)


if __name__ == '__main__':
    main()

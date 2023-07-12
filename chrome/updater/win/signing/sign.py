#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An utility for signing an updater metainstaller and contained code. This
utility can also be used to package and sign an offline metainstaller by
specifying the `--appid`, `--installer_path` and `--manifest_path` arguments.

For example, to sign `UpdaterSetup.exe`:
python3 sign.py --in_file UpdaterSetup.exe --out_file UpdaterSetup.signed.exe

Or, for example, to package and sign an offline metainstaller:

```
python3 sign.py --in_file UpdaterSetup.exe --out_file ChromeOfflineSetup.exe
    --appid {8A69D345-D564-463c-AFF1-A69D9E530F96}
    --installer_path path/to/110.0.5478.0_chrome_installer.exe
    --manifest_path path/to/OfflineManifest.gup
```

Or, for example, to package and sign an offline metainstaller with a local PFX
file:
```
python3 sign.py --in_file UpdaterSetup.exe --out_file ChromeOfflineSetup.exe
    --certificate_file_path path/to/TestCert.pfx
    --certificate_password TestCertPassword
    --appid {8A69D345-D564-463c-AFF1-A69D9E530F96}
    --installer_path path/to/110.0.5478.0_chrome_installer.exe
    --manifest_path path/to/OfflineManifest.gup
```

`OfflineManifest.gup` can include the following replaceable parameters that will
be replaced with the computed values:
* `${INSTALLER_SIZE}`.
* `${INSTALLER_HASH_SHA256}`.

To run locally with a certificate in `My` store:
 1. Create a self-signed developer certificate if you haven't yet by executing
 `New-SelfSignedCertificate -DnsName id@domain.tld -Type CodeSigning
 -CertStoreLocation cert:\CurrentUser\My` in powershell.
 2. Run `autoninja -C .\out\yourBuildDir chrome/updater`.
 3. cd to your build dir and execute this script, specifying the above
 certificate in the `--identity`.
"""

import argparse
import array
import hashlib
import os
import shutil
import subprocess
import tempfile
import uuid

import resedit


class SigningError(Exception):
    """Module exception class."""


class Signer:
    """A container for a signing operation."""

    def __init__(self, tmpdir, lzma_exe, signtool_exe, tagging_exe, identity,
                 certificate_file_path, certificate_password):
        """Inits a signer with the necessary tools."""
        self._tmpdir = tmpdir
        self._lzma_exe = lzma_exe
        self._signtool_exe = signtool_exe
        self._tagging_exe = tagging_exe
        self._identity = identity
        self._certificate_file_path = certificate_file_path
        self._certificate_password = certificate_password

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
            'http://timestamp.digicert.com', '/td', 'SHA256', '/fd', 'SHA256'
        ]
        if self._certificate_file_path:
            command += ['/f', self._certificate_file_path]
            if self._certificate_password:
                command += ['/p', self._certificate_password]
        else:
            command += ['/s', 'My', '/n', self._identity]

        command += [in_file]
        subprocess.run(command, check=True)

    def _generate_target_manifest(self, installer_path, manifest_path):
        """Replaces `${INSTALLER_SIZE}` and `${INSTALLER_HASH_SHA256}` in the
        manifest with the computed size and hash, and returns the resultant
        string."""
        size = os.stat(os.path.abspath(installer_path)).st_size
        data = array.array('B')
        with open(os.path.abspath(installer_path), 'rb') as installer_file:
            data.fromfile(installer_file, size)

        with open(manifest_path, 'rt') as f:
            manifest_result = f.read()
            for key, value in {
                    '${INSTALLER_SIZE}': str(size),
                    '${INSTALLER_HASH_SHA256}':
                    hashlib.sha256(data).hexdigest()
            }.items():
                manifest_result = manifest_result.replace(key, value)
        return manifest_result

    def _copy_offline_installer_files(self, root, appid, installer_path,
                                      manifest_path):
        """Copies the offline installer and generated manifest under `root`."""
        offline_dir = os.path.join(root, 'Offline',
                                   '{' + str(uuid.uuid4()) + '}')
        app_dir = os.path.join(offline_dir, appid)
        os.makedirs(app_dir)

        with open(os.path.join(offline_dir, 'OfflineManifest.gup'),
                  'w') as f_out:
            f_out.write(
                self._generate_target_manifest(installer_path, manifest_path))
        shutil.copyfile(
            installer_path,
            os.path.join(app_dir, os.path.basename(installer_path)))

    def _sign_7z(self, in_file, appid, installer_path, manifest_path):
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
                    self._sign_7z(os.path.join(root, f), appid, installer_path,
                                  manifest_path)
            if appid and 'updater.exe' in files:
                self._copy_offline_installer_files(root, appid, installer_path,
                                                   manifest_path)
        subprocess.run([self._lzma_exe, 'a', '-mx0', in_file, '*'],
                       check=True,
                       cwd=tmp)

    def sign_metainstaller(self,
                           in_file,
                           appid=None,
                           installer_path=None,
                           manifest_path=None):
        """Return a path to a signed copy of an updater metainstaller."""
        workdir = tempfile.mkdtemp(dir=self._tmpdir)
        out_metainstaller = os.path.join(workdir, "metainstaller.exe")
        resed = resedit.ResourceEditor(in_file, out_metainstaller)
        resource = 'updater.packed.7z'
        extracted_7z = os.path.join(workdir, resource)
        resed.ExtractResource('B7', 1033, resource, extracted_7z)
        self._sign_7z(extracted_7z, appid, installer_path, manifest_path)
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
    parser.add_argument(
        '--identity',
        default='Google',
        help=('The subject name of the signing certificate in the `My` store. '
              'Can be a substring of the entire subject name.'))
    parser.add_argument(
        '--certificate_file_path',
        required=False,
        help=('Specifies the path to a PFX signing certificate. Takes '
              'precedence over `--identity`.'))
    parser.add_argument(
        '--certificate_password',
        required=False,
        help='Specifies the password for `--certificate_file_path`.')
    parser.add_argument('--appid',
                        required=False,
                        help='The offline installer appid.')
    parser.add_argument('--installer_path',
                        required=False,
                        help='The path to the offline installer.')
    parser.add_argument('--manifest_path',
                        required=False,
                        help='The path to the offline manifest .gup file.')
    args = parser.parse_args()
    if args.appid and (args.installer_path is None
                       or args.manifest_path is None):
        parser.error(
            '`--appid` requires `--installer_path` and `--manifest_path`.')
    if args.certificate_password and args.certificate_file_path is None:
        parser.error(
            '`--certificate_password` requires `--certificate_file_path`.')

    with tempfile.TemporaryDirectory() as tmpdir:
        shutil.move(
            Signer(tmpdir, args.lzma_7z, args.signtool, args.certificate_tag,
                   args.identity, args.certificate_file_path,
                   args.certificate_password).sign_metainstaller(
                       args.in_file, args.appid, args.installer_path,
                       args.manifest_path), args.out_file)


if __name__ == '__main__':
    main()

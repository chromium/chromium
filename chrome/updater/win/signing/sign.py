#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An utility for signing an updater metainstaller and contained code. This
utility can also be used to package and sign an offline metainstaller by
specifying the `--appid`, `--installer_path` and `--manifest_path` arguments,
along with the optional `--manifest_dict_replacements` argument.

For example, to sign `UpdaterSetup.exe`:
python3 sign.py --in_file UpdaterSetup.exe --out_file UpdaterSetup.signed.exe

Or, for example, to package and sign an offline metainstaller:

```
python3 sign.py --in_file UpdaterSetup.exe --out_file ChromeOfflineSetup.exe
    --appid {8A69D345-D564-463c-AFF1-A69D9E530F96}
    --installer_path path/to/110.0.5478.0_chrome_installer.exe
    --manifest_path path/to/OfflineManifest.gup
    --manifest_dict_replacements
        "{'${INSTALLER_VERSION}':'110.0.5478.0', '${ARCH_REQUIREMENT}':'x86'}"
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
    --manifest_dict_replacements
        "{'${INSTALLER_VERSION}':'110.0.5478.0', '${ARCH_REQUIREMENT}':'x86'}"
```

In addition to the keys found in `manifest_dict_replacements`,
`OfflineManifest.gup` can also include the following replaceable parameters that
will be replaced with values computed by this script:
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
import ast
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import uuid

import resedit


class SigningError(Exception):
    """Module exception class."""


class Signer:
    """A container for a signing operation."""
    def __init__(self, tmpdir, lzma_exe, signtool_exe, tagging_exe, identity,
                 certificate_file_path, certificate_password, sign_flags):
        """Inits a signer with the necessary tools.

        Arguments:
        tmpdir - a path to a temp dir to use.
        lzma_exe - a path to a lzma executable (7zr.exe works)
        signtool_exe - a path to a signtool.exe executable
        tagging_exe - a path to the tagging executable
        identity - optional. If provided, this identity will be appended to the
          signing flags for each signing.
        certificate_file_path - optional. If provided, this path will be
          appended to the signing flags for each signing.
        certificate_password - optional. If provided, this password will be
          appended to the signing flags for each signing.
        sign_flags - a list of lists. The executable will be signed once per
          entry in this list, using the provided flags with the additions above.
          This enables signing with multiple certificates.
        """
        self._tmpdir = tmpdir
        self._lzma_exe = lzma_exe
        self._signtool_exe = signtool_exe
        self._tagging_exe = tagging_exe
        self._identity = identity
        self._certificate_file_path = certificate_file_path
        self._certificate_password = certificate_password
        self._sign_flags = sign_flags

    def _add_tagging_cert(self, in_file, out_file):
        """Adds the tagging cert. Returns the path to the tagged file."""
        subprocess.run(
            [self._tagging_exe, '--set-tag',
             '--out=%s' % out_file, in_file],
            check=True)
        return out_file

    def _sign_item(self, in_file):
        """Sign an executable in-place."""
        # Retries may be required: lore states the timestamp server is flaky.
        for flags in self._sign_flags:
            command = [self._signtool_exe, 'sign']
            command += flags
            if self._certificate_file_path:
                command += ['/f', self._certificate_file_path]
            if self._certificate_password:
                command += ['/p', self._certificate_password]
            if self._identity:
                command += ['/s', 'My', '/n', self._identity]

            command += [in_file]
            subprocess.run(command, check=True)

    def _generate_target_manifest(self, appid, installer_path, manifest_path,
                                  manifest_dict_replacements):
        """Replaces the following in the input file `manifest_path`:
        * `${APP_ID}`: `appid`.
        * `${INSTALLER_FILENAME}`: base name of `installer_path`.
        * `${INSTALLER_SIZE}`: computed size of `installer_path`.
        * `${INSTALLER_HASH_SHA256}`: computed sha256 hash of `installer_path`.
        * Keys in `manifest_dict_replacements` with the corresponding values.
        Returns the resultant manifest as a string."""
        size = os.stat(os.path.abspath(installer_path)).st_size
        data = array.array('B')
        with open(os.path.abspath(installer_path), 'rb') as installer_file:
            data.fromfile(installer_file, size)

        with open(manifest_path, 'rt') as f:
            manifest_result = f.read()
            for key, value in {
                    '${APP_ID}': appid,
                    '${INSTALLER_FILENAME}': os.path.basename(installer_path),
                    '${INSTALLER_SIZE}': str(size),
                    '${INSTALLER_HASH_SHA256}':
                    hashlib.sha256(data).hexdigest(),
            }.items():
                manifest_result = manifest_result.replace(key, value)
            for key, value in manifest_dict_replacements.items():
                manifest_result = manifest_result.replace(key, value)
        return manifest_result

    def _copy_offline_installer_files(self, root, appid, installer_path,
                                      manifest_path,
                                      manifest_dict_replacements):
        """Copies the offline installer and generated manifest under `root`."""
        offline_dir = os.path.join(root, 'Offline',
                                   '{' + str(uuid.uuid4()) + '}')
        app_dir = os.path.join(offline_dir, appid)
        os.makedirs(app_dir)

        with open(os.path.join(offline_dir, 'OfflineManifest.gup'),
                  'w') as f_out:
            f_out.write(
                self._generate_target_manifest(appid, installer_path,
                                               manifest_path,
                                               manifest_dict_replacements))
        shutil.copyfile(
            installer_path,
            os.path.join(app_dir, os.path.basename(installer_path)))

    def _sign_7z(self, in_file, appid, installer_path, manifest_path,
                 manifest_dict_replacements):
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
                                  manifest_path, manifest_dict_replacements)
            if appid and 'updater.exe' in files:
                self._copy_offline_installer_files(root, appid, installer_path,
                                                   manifest_path,
                                                   manifest_dict_replacements)
        subprocess.run([self._lzma_exe, 'a', '-mx0', in_file, '*'],
                       check=True,
                       cwd=tmp)

    def sign_metainstaller(self,
                           in_file,
                           out_file,
                           appid=None,
                           installer_path=None,
                           manifest_path=None,
                           manifest_dict_replacements=None):
        """Return a path to a signed copy of an updater metainstaller."""
        workdir = tempfile.mkdtemp(dir=self._tmpdir)
        out_metainstaller = os.path.join(workdir, "metainstaller.exe")
        resed = resedit.ResourceEditor(in_file, out_metainstaller)
        resource = 'updater.packed.7z'
        extracted_7z = os.path.join(workdir, resource)
        resed.ExtractResource('B7', 1033, resource, extracted_7z)
        self._sign_7z(extracted_7z, appid, installer_path, manifest_path,
                      manifest_dict_replacements)
        resed.UpdateResource('B7', 1033, resource, extracted_7z)
        resed.Commit()
        self._sign_item(out_metainstaller)
        return self._add_tagging_cert(out_metainstaller, out_file)


def has_switch(switch_name: str) -> bool:
    return any(switch.startswith(switch_name) for switch in sys.argv)


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
                        required=not shutil.which('7z.exe'),
                        help=('The path to the 7z executable.'
                              'Required when 7z.exe is not in the PATH.'))
    parser.add_argument(
        '--signtool',
        default='signtool.exe',
        help='The path to the signtool executable. Look in depot_tools.')
    parser.add_argument('--tagging_exe',
                        default=os.path.join(
                            os.path.realpath(os.path.dirname(__file__)),
                            'tag.exe'),
                        help='The path to the tagging executable.')
    parser.add_argument(
        '--identity',
        default='Google',
        help=('The subject name of the signing certificate in the `My` store. '
              'Can be a substring of the entire subject name.'))
    parser.add_argument(
        '--certificate_file_path',
        required=has_switch('--certificate_password'),
        help=('Specifies the path to a PFX signing certificate. Takes '
              'precedence over `--identity`.'
              'Required when `--certificate_password` is present.'))
    parser.add_argument(
        '--certificate_password',
        required=False,
        help='Specifies the password for `--certificate_file_path`.')
    parser.add_argument('--appid',
                        required=False,
                        help='The offline installer appid.')
    parser.add_argument('--installer_path',
                        required=has_switch('--appid'),
                        help=('The path to the offline installer.'
                              'Required when `--appid` is present.'))
    parser.add_argument(
        '--manifest_path',
        required=(has_switch('--appid')
                  or has_switch('--manifest_dict_replacements')),
        help=('The path to the offline manifest .gup file.'
              'Required when `--appid` or '
              '`--manifest_dict_replacements` is present.'))
    parser.add_argument(
        '--manifest_dict_replacements',
        default='{}',
        help=('A dictionary of `{key1:value1, ...keyN:valueN}`. This script '
              'replaces the keys that it finds in the offline manifest .gup '
              'file with the corresponding values.'))
    parser.add_argument('--sign_flags',
                        action='append',
                        default=[],
                        help='Flags to pass to codesign.exe.')
    args = parser.parse_args()
    sign_flags = args.sign_flags or [
        '/v', '/tr', 'http://timestamp.digicert.com', '/td', 'SHA256', '/fd',
        'SHA256'
    ]
    with tempfile.TemporaryDirectory() as tmpdir:
        Signer(tmpdir, args.lzma_7z, args.signtool, args.tagging_exe,
               args.identity, args.certificate_file_path,
               args.certificate_password, [sign_flags]).sign_metainstaller(
                   args.in_file, args.out_file, args.appid,
                   args.installer_path, args.manifest_path,
                   ast.literal_eval(args.manifest_dict_replacements))


if __name__ == '__main__':
    main()

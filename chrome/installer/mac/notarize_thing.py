#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

sys.path.append(os.path.dirname(__file__))

from signing import notarize
from signing.config import CodeSignConfig
from signing.model import NotarizationTool


def main():
    parser = argparse.ArgumentParser(
        description='Notarize and staple an application binary or archive.')
    parser.add_argument(
        '--user',
        '-u',
        required=True,
        help='The username to access the Apple notary service.')
    parser.add_argument(
        '--password',
        '-p',
        required=True,
        help='The password or password reference (e.g. @keychain, see '
        '`xcrun altool -h`) to access the Apple notary service.')
    parser.add_argument(
        '--asc-provider',
        help='The ASC provider string to be used as the `--asc-provider` '
        'argument to `xcrun altool`, to be used when --user is associated with '
        'with multiple Apple developer teams. See `xcrun altool -h`. Run '
        '`iTMSTransporter -m provider -account_type itunes_connect -v off -u '
        'USERNAME -p PASSWORD` to list valid providers.')
    parser.add_argument(
        '--team-id',
        help='The Apple Developer Team ID used to authenticate to the Apple '
        'notary service.')
    parser.add_argument(
        '--notarization-tool',
        choices=list(NotarizationTool),
        type=NotarizationTool,
        default=NotarizationTool.ALTOOL,
        help='The tool to use to communicate with the Apple notary service.')
    parser.add_argument(
        '--no-staple',
        action='store_true',
        help='Wait for notarization results, but do not staple after '
        'successful notarization.')
    parser.add_argument(
        '--bundle-id',
        required=False,
        help='Force the use of the specified bundle ID when uploading for '
        'notarization, rather than the one from a config.')
    parser.add_argument(
        'file',
        nargs='+',
        help='The file(s) to have notarized. Each file can be a zipped .app '
        'bundle, a .dmg, or a .pkg. `xcrun altool -h` for information on '
        'supported formats.')
    args = parser.parse_args()

    if args.notarization_tool == NotarizationTool.NOTARYTOOL:
        if not args.team_id:
            parser.error('The `--notarization-tool=notarytool` option requires '
                         'a --team-id.')

    config_class = CodeSignConfig
    if args.bundle_id:

        class OverrideBundleIDConfig(CodeSignConfig):

            @property
            def base_bundle_id(self):
                return args.bundle_id

        config_class = OverrideBundleIDConfig

    config = config_class(
        identity='notused',
        installer_identity='notused',
        notary_user=args.user,
        notary_password=args.password,
        notary_asc_provider=args.asc_provider,
        notary_team_id=args.team_id,
        notarization_tool=args.notarization_tool)

    uuids = []
    for path in args.file:
        print('Submitting {} for notarization'.format(path))
        uuid = notarize.submit(path, config)
        uuids.append(uuid)

    for uuid in notarize.wait_for_results(uuids, config):
        print('Notarization results acquired for request {}'.format(uuid))

    if not args.no_staple:
        for path in args.file:
            print('Stapling notarization ticket for {}'.format(path))
            notarize.staple(path)


if __name__ == '__main__':
    main()

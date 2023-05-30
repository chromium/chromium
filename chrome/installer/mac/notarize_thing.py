#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

sys.path.append(os.path.dirname(__file__))

from signing import invoker, notarize
from signing.config import CodeSignConfig
from signing.model import NotarizationTool


def main():
    parser = argparse.ArgumentParser(
        description='Notarize and staple an application binary or archive.')
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

    notarize.Invoker.register_arguments(parser)
    args = parser.parse_args()

    config_class = CodeSignConfig
    if args.bundle_id:

        class OverrideBundleIDConfig(CodeSignConfig):

            @property
            def base_bundle_id(self):
                return args.bundle_id

        config_class = OverrideBundleIDConfig

    if not args.notarization_tool:
        args.notarization_tool = NotarizationTool.ALTOOL

    config = config_class(
        identity='notused',
        invoker=lambda config: NotarizationInvoker(args, config))

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


class NotarizationInvoker(invoker.Interface):

    def __init__(self, args, config):
        self._notarizer = notarize.Invoker(args, config)

    @property
    def notarizer(self):
        return self._notarizer


if __name__ == '__main__':
    main()

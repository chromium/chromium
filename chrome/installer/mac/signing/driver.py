# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The driver module provides the command line interface to the signing module.
"""

import argparse
import os

from signing import config_factory, commands, invoker, logger, model, pipeline


def _create_config(config_args, development):
    """Creates the |model.CodeSignConfig| for the signing operations.

    If |development| is True, the config will be modified to not require
    restricted internal assets, nor will the products be required to match
    specific certificate hashes.

    Args:
        config_args: Dict of args to expand as kwargs to the config class's
            constructor.
        development: Boolean indicating whether or not to modify the chosen
            config for development testing.

    Returns:
        An instance of |model.CodeSignConfig|.
    """
    config_class = config_factory.get_class()

    if development:

        class DevelopmentCodeSignConfig(config_class):

            @property
            def codesign_requirements_basic(self):
                return ''

            @property
            def codesign_requirements_outer_app(self):
                return ''

            @property
            def provisioning_profile_basename(self):
                return None

            @property
            def run_spctl_assess(self):
                # Self-signed or ad-hoc signed signing identities won't pass
                # spctl assessment so don't do it.
                return False

            @property
            def inject_get_task_allow_entitlement(self):
                return True

            @property
            def main_executable_pinned_geometry(self):
                # Pinned geometry is only needed to keep release builds
                # consistent over time. Ignore executable geometry when code
                # signing for development.
                return None

        config_class = DevelopmentCodeSignConfig

    return config_class(**config_args)


def _show_tool_versions():
    logger.info('Showing macOS and tool versions.')
    commands.run_command(['sw_vers'])
    commands.run_command(['xcodebuild', '-version'])
    commands.run_command(['xcrun', '-show-sdk-path'])


def main(args):
    """Runs the signing pipeline.

    Args:
        args: List of command line arguments.
    """
    parser = argparse.ArgumentParser(
        description='Code sign and package Chrome for channel distribution.')
    parser.add_argument(
        '--identity',
        required=True,
        help='The identity to sign everything but PKGs with.')
    parser.add_argument(
        '--installer-identity', help='The identity to sign PKGs with.')
    parser.add_argument(
        '--development',
        action='store_true',
        help='The specified identity is for development. Certain codesign '
        'requirements will be omitted.')
    parser.add_argument(
        '--input',
        required=True,
        help='Path to the input directory. The input directory should '
        'contain the products to sign, as well as the Packaging directory.')
    parser.add_argument(
        '--output',
        required=True,
        help='Path to the output directory. The signed (possibly packaged) '
        'products and installer tools will be placed here.')
    parser.add_argument(
        '--disable-packaging',
        action='store_true',
        help='Disable creating any packaging (.dmg/.pkg) specified by the '
        'configuration.')
    parser.add_argument(
        '--skip-brand',
        dest='skip_brands',
        action='append',
        default=[],
        help='Causes any distribution whose brand code matches to be skipped. '
        'A value of * matches all brand codes.')
    parser.add_argument(
        '--channel',
        dest='channels',
        action='append',
        default=[],
        help='If provided, only the distributions matching the specified '
        'channel(s) will be produced. The string "stable" matches the None '
        'channel.')
    parser.add_argument(
        '--notarize',
        nargs='?',
        choices=list(model.NotarizeAndStapleLevel),
        const='staple',
        default='none',
        type=model.NotarizeAndStapleLevel.from_string,
        help='Specifies the requested notarization actions to be taken. '
        '`none` causes no notarization tasks to be performed. '
        '`nowait` submits the signed application and packaging to Apple for '
        'notarization, but does not wait for a reply. '
        '`wait-nostaple` submits the signed application and packaging to Apple '
        'for notarization, and waits for a reply, but does not staple the '
        'resulting notarization ticket. '
        '`staple` submits the signed application and packaging to Apple for '
        'notarization, waits for a reply, and staples the resulting '
        'notarization ticket. '
        'If the `--notarize` argument is not present, that is the equivalent '
        'of `--notarize none`. If the `--notarize` argument is present but '
        'has no option specified, that is the equivalent of `--notarize '
        'staple`.')

    invoker_cls = config_factory.get_invoker_class()
    invoker_cls.register_arguments(parser)

    args = parser.parse_args(args)

    config_args = model.pick(args, (
        'identity',
        'installer_identity',
        'notarize',
    ))

    def _create_invoker(config):
        try:
            return invoker_cls(args, config)
        except invoker.InvokerConfigError as e:
            parser.error(str(e))

    config_args['invoker'] = _create_invoker
    config = _create_config(config_args, args.development)
    paths = model.Paths(args.input, args.output, None)

    if not commands.file_exists(paths.output):
        commands.make_dir(paths.output)

    _show_tool_versions()

    pipeline.sign_all(
        paths,
        config,
        disable_packaging=args.disable_packaging,
        skip_brands=args.skip_brands,
        channels=args.channels)

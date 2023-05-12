# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The signing module defines the various binary pieces of the Chrome application
bundle that need to be signed, as well as providing utilities to sign them.
"""

import os.path
import re

from . import commands


def _linker_signed_arm64_needs_force(path):
    """Detects linker-signed arm64 code that can only be signed with --force
    on this system.

    Args:
        path: A path to a code object to test.

    Returns:
        True if --force must be used with codesign --sign to successfully sign
        the code, False otherwise.
    """
    # On macOS 11.0 and later, codesign handles linker-signed code properly
    # without the --force hand-holding. Check OS >= 10.16 because that's what
    # Python will think the OS is if it wasn't built with the 11.0 SDK or later.
    if commands.macos_version() >= [10, 16]:
        return False

    # Look just for --arch=arm64 because that's the only architecture that has
    # linker-signed code by default. If this were used with universal code (if
    # there were any), --display without --arch would default to the native
    # architecture, which almost certainly wouldn't be arm64 and therefore would
    # be wrong.
    (returncode, stdout, stderr) = commands.lenient_run_command_output(
        ['codesign', '--display', '--verbose', '--arch=arm64', '--', path])

    if returncode != 0:
        # Problem running codesign? Don't make the error about this confusing
        # function. Just return False and let some less obscure codesign
        # invocation be the error. Not signed at all? No problem. No arm64 code?
        # No problem either. Not code at all? File not found? Well, those don't
        # count as linker-signed either.
        return False

    # Yes, codesign --display puts all of this on stderr.
    match = re.search(b'^CodeDirectory .* flags=(0x[0-9a-f]+)( |\().*$', stderr,
                      re.MULTILINE)
    if not match:
        return False

    flags = int(match.group(1), 16)

    # This constant is from MacOSX11.0.sdk <Security/CSCommon.h>
    # SecCodeSignatureFlags kSecCodeSignatureLinkerSigned.
    LINKER_SIGNED_FLAG = 0x20000

    return (flags & LINKER_SIGNED_FLAG) != 0


def sign_part(paths, config, part):
    """Code signs a part.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| to sign. The product's |path| must
            be in |paths.work|.
    """
    command = ['codesign', '--sign', config.identity]
    path = os.path.join(paths.work, part.path)
    if _linker_signed_arm64_needs_force(path):
        command.append('--force')
    if config.notary_user:
        # Assume if the config has notary authentication information that the
        # products will be notarized, which requires a secure timestamp.
        command.append('--timestamp')
    if part.sign_with_identifier:
        command.extend(['--identifier', part.identifier])
    reqs = part.requirements_string(config)
    if reqs:
        command.extend(['--requirements', '=' + reqs])
    if part.options:
        command.extend(['--options', part.options.to_comma_delimited_string()])
    if part.entitlements:
        command.extend(
            ['--entitlements',
             os.path.join(paths.work, part.entitlements)])
    command.append(path)
    commands.run_command(command)


def verify_part(paths, part):
    """Displays and verifies the code signature of a part.

    Args:
        paths: A |model.Paths| object.
        part: The |model.CodeSignedProduct| to verify. The product's |path|
            must be in |paths.work|.
    """
    verify_options = part.verify_options.to_list(
    ) if part.verify_options else []
    part_path = os.path.join(paths.work, part.path)
    commands.run_command([
        'codesign', '--display', '--verbose=5', '--requirements', '-', part_path
    ])
    commands.run_command(['codesign', '--verify', '--verbose=6'] +
                         verify_options + [part_path])


def validate_app(paths, config, part):
    """Displays and verifies the signature of a CodeSignedProduct.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| for the outer application bundle.
    """
    app_path = os.path.join(paths.work, part.path)
    commands.run_command([
        'codesign', '--display', '--requirements', '-', '--verbose=5', app_path
    ])
    if config.run_spctl_assess:
        commands.run_command(['spctl', '--assess', '-vv', app_path])

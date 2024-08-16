# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The signing module defines the various binary pieces of the Chrome application
bundle that need to be signed, as well as providing utilities to sign them.
"""

import os.path
import re

from signing import commands, invoker, logger


class InvalidLipoArchCountException(ValueError):
    pass


class InvalidAppGeometryException(ValueError):
    pass


class Invoker(invoker.Base):

    def codesign(self, config, product, path):
        command = ['codesign', '--sign', config.identity]
        if config.notarize.should_notarize():
            # If the products will be notarized, the signature requires a secure
            # timestamp.
            command.append('--timestamp')
        if product.sign_with_identifier:
            command.extend(['--identifier', product.identifier])
        reqs = product.requirements_string(config)
        if reqs:
            command.extend(['--requirements', '=' + reqs])
        if product.options:
            command.extend(
                ['--options',
                 product.options.to_comma_delimited_string()])
        if product.entitlements:
            command.extend(
                ['--entitlements',
                 os.path.join(path, product.entitlements)])
        command.append(os.path.join(path, product.path))
        commands.run_command(command)


def _binary_architectures_offsets(binary_path):
    """Returns a tuple of architecture offset pairs.
    Raises InvalidLipoArchCountException if the nfat_arch count does not match
    the parsed architecture count.

    Args:
        binary_path: The path to the binary on disk.

    Returns:
        Returns a tuple of architecture offset pairs.
        For example: (('x86_64', 16384), ('arm64', 294912))
        or for non-universal: (('arm64', 0),)
    """
    command = ['lipo', '-detailed_info', binary_path]
    output = commands.run_command_output(command)

    # Log the detailed macho info. This includes the slice sizes, which will
    # be helpful when recalculating padding for https://crbug.com/1300598.
    logger.info('%s', output.decode('utf-8'))

    # Find the architecture for a non-universal binary.
    match = re.search(rb'^Non-fat file:.+architecture:\s(.+)$', output,
                      re.MULTILINE)
    if match is not None:
        return tuple(((match.group(1).decode('ascii'), 0),))

    # Get the expected number of architectures for a universal binary.
    nfat_arch_count = int(
        re.search(rb'^nfat_arch\s(\d+)$', output, re.MULTILINE).group(1))

    # Find architecture-offset pairs for a universal binary.
    arch_offsets = tuple(
        (match.group(1).decode('ascii'), int(match.group(2)))
        for match in re.finditer(
            rb'^architecture\s(.+)\n(?:^[^\n]*\n)*?^\s+offset\s(\d+)$', output,
            re.MULTILINE)
    )

    # Make sure nfat_arch matches the found number of pairs.
    if nfat_arch_count != len(arch_offsets):
        raise InvalidLipoArchCountException()

    return arch_offsets


def sign_part(paths, config, part):
    """Code signs a part.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| to sign. The product's |path| must
            be in |paths.work|.
    """
    config.invoker.signer.codesign(config, part, paths.work)


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


def validate_app_geometry(paths, config, part):
    """Validates the architecture offsets in the main executable.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| for the outer application bundle.
    """
    if config.main_executable_pinned_geometry is None:
        return

    app_binary_path = os.path.join(paths.work, config.app_dir, 'Contents',
                                   'MacOS', config.app_product)
    pinned_offsets = config.main_executable_pinned_geometry
    offsets = _binary_architectures_offsets(app_binary_path)
    if pinned_offsets != offsets:
        raise InvalidAppGeometryException('Unexpected main executable geometry',
                                          pinned_offsets, offsets)

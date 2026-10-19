# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The parts module defines the various binary pieces of the Views Examples
application bundle that need to be signed, as well as providing a function
to sign them.
"""

from signing import signing
from signing.model import CodeSignOptions, CodeSignedProduct, VerifyOptions


def get_parts(config):
    """Returns all the |model.CodeSignedProduct| objects to be signed for a
    Views Examples application bundle.

    Args:
        config: The |config.CodeSignConfig|.

    Returns:
        A list of |model.CodeSignedProduct|. Items should be signed in the
        order they appear in this list.
    """
    # Innermost parts come first.
    return [
        CodeSignedProduct(
            (
                '{0.framework_dir}/Versions/Current/Libraries'
                '/libtest_trace_processor.dylib'
            ).format(config),
            # As for Chrome's dylibs, the identifier is the file name without
            # the extension, which is what codesign infers when signing. The
            # two must match for the designated requirement to be satisfied.
            'libtest_trace_processor',
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT,
        ),
        CodeSignedProduct(
            '{0.framework_dir}'.format(config),
            '{}.framework'.format(config.base_bundle_id),
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT,
        ),
        CodeSignedProduct(
            '{0.app_product}.app'.format(config),
            config.base_bundle_id,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            requirements=config.codesign_requirements_outer_app,
            identifier_requirement=False,
            entitlements=None,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT,
        ),
    ]


async def sign_all(paths, config):
    """Code signs the application bundle and all of its internal nested code
    parts.

    Args:
        paths: A |model.Paths| object.
        config: The |model.CodeSignConfig| object. The |app_product| binary and
            nested binaries must exist in |paths.work|.
    """
    parts = get_parts(config)
    for part in parts:
        signing.sign_part(paths, config, part)
        signing.verify_part(paths, part)
    await signing.validate_app(paths, config, parts[-1])

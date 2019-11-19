# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .build_props_config import BuildPropsCodeSignConfig


class ChromiumCodeSignConfig(BuildPropsCodeSignConfig):
    """A CodeSignConfig used for signing non-official Chromium builds.

    This is primarily used for testing, so it does not include certain
    signing elements like provisioning profiles. It also omits internal-only
    resources.
    """

    @property
    def optional_parts(self):
        # This part requires src-internal, so it is not required for a Chromium
        # build signing.
        return set(('libwidevinecdm.dylib',))

    @property
    def provisioning_profile_basename(self):
        return None

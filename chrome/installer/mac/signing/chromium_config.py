# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from signing.build_props_config import BuildPropsCodeSignConfig


class ChromiumCodeSignConfig(BuildPropsCodeSignConfig):
    """A CodeSignConfig used for signing non-official Chromium builds.

    This is primarily used for testing, so it does not include certain
    signing elements like provisioning profiles.
    """

    @property
    def provisioning_profile_basename(self):
        return None

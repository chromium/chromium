# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from signing.build_props_config import BuildPropsCodeSignConfig


class UnbrandedCodeSignConfig(BuildPropsCodeSignConfig):
    """A CodeSignConfig used for signing non-official Updater builds."""

    @property
    def packaging_dir(self):
        return 'Updater Packaging'

    @property
    def provisioning_profile_basename(self):
        return None

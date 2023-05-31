# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def get_class():
    """Returns the subclass of |model.CodeSignConfig| to use."""
    # First look up the processed Chromium config.
    from signing.chromium_config import ChromiumCodeSignConfig
    config_class = ChromiumCodeSignConfig

    # Then search for the internal config for Google Chrome.
    try:
        from signing.internal_config import InternalCodeSignConfig
        config_class = InternalCodeSignConfig
    except ImportError as e:
        # If the build specified Google Chrome as the product, then the
        # internal config has to be available.
        if config_class.is_chrome_branded():
            raise e

    return config_class


def get_invoker_class():
    """Returns the subclass of |invoker.Interface| to use."""
    try:
        from signing.internal_invoker import Invoker
        return Invoker
    except ImportError as e:
        # If the build specified Google Chrome as the product, then the
        # internal invoker has to be available.
        if get_class().is_chrome_branded():
            raise e

    from signing.standard_invoker import Invoker
    return Invoker

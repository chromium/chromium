# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def get_class():
    """Returns the subclass of |model.CodeSignConfig| to use."""
    from signing.unbranded_config import UnbrandedCodeSignConfig
    return UnbrandedCodeSignConfig


def get_invoker_class():
    """Returns the subclass of |invoker.Interface| to use."""
    try:
        from signing.internal_invoker import Invoker
        return Invoker
    except ImportError as e:
        pass

    from signing.standard_invoker import Invoker
    return Invoker

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def get_class():
    """Returns the subclass of |model.CodeSignConfig| to use."""
    from unbranded_config import UnbrandedCodeSignConfig
    return UnbrandedCodeSignConfig

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def import_mock():
    # python2 support.
    try:
        from unittest import mock
        return mock
    except:
        import mock
        return mock

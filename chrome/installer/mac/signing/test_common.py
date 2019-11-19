# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def import_mock():
    # python2 support.
    try:
        from unittest import mock
        return mock
    except:
        import os.path
        import sys
        sys.path.append(
            os.path.join(
                os.path.dirname(__file__), os.path.join(*((os.pardir,) * 4)),
                'third_party', 'pymock'))
        import mock
        return mock

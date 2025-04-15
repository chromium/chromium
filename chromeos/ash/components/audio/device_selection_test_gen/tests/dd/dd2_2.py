# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C, D = s.abc(T.Internal, T.HDMI, T.Headphone, T.HDMI)
    s.plug(A)

    # The user plugs monitor B.
    s.plug(B)

    # The user plugs the headphone C.
    s.plug(C)

    # The user selects monitor B as the activated device.
    s.select(B)

    # The user plugs the monitor D. => The activated device should be D
    s.plug(D)
    s.expect(D)

    # The user selects the monitor D. => The activated device should be D
    s.select(D)

    # The user unplugs the monitor D. => The activated device should be B
    s.unplug(D)
    s.expect(B)

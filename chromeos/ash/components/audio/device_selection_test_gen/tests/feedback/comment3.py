# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C, D = s.abc(T.Internal, T.HDMI, T.HDMI, T.USB)
    s.plug(A)

    # The user plugs monitor 1.
    s.plug(B)

    # The user plugs monitor 2.
    s.plug(C)

    # The user plugs a line out speaker.
    s.plug(D)

    # The user selects the line out speaker as the activated device.
    s.select(D)

    # The user unplugs monitor 1.
    s.unplug(B)

    # The user unplugs monitor 2.
    s.unplug(C)

    # The user unplugs a line out speaker.
    s.unplug(D)

    # The user plugs monitor 1.
    s.plug(B)

    # The user plugs monitor 2.
    s.plug(C)

    # The user plugs a line out speaker. => The activated device should be the line out speaker.
    s.plug(D)
    s.expect(D)

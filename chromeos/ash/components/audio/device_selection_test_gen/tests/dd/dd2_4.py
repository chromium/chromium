# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C, D = s.abc(T.Internal, T.HDMI, T.HDMI, T.HDMI)
    s.plug(A)

    # The user plugs a external monitor B => The activated device should be B
    s.plug(B)
    s.expect(B)

    # The user plugs a external monitor D => The activated device should be D
    s.plug(D)
    s.expect(D)

    # The user plugs a external monitor C => The activated device should be C
    s.plug(C)
    s.expect(C)

    # The user selects external monitor D as the activated device
    s.select(D)

    # The user unplugs external monitor D
    s.unplug(D)

    # The user selects internal speaker A as the activated device
    s.select(A)

    # The user plugs external monitor D => The activated device should be D
    s.plug(D)
    s.expect(D)

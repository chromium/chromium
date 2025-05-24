# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.Headphone)
    s.plug(A)

    # The user plugs a monitor B
    s.plug(B)

    # The user selects the internal speaker A as the activate device
    s.select(A)

    # The user plugs a headphone C => The activated device should be C
    s.plug(C)
    s.expect(C)

    # The user unplugs the headphone C => The activated device should be A
    s.unplug(C)
    s.expect(A)

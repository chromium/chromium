# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C, I = s.abc(T.HDMI, T.HDMI, T.Headphone, T.Internal, charset='ABCI')
    s.plug(I)

    # The user plugs a external monitor A
    s.plug(A)
    # The user plugs a external monitor B
    s.plug(B)
    # The user selects the external monitor A as the activated device
    s.select(A)
    # The user unplugs the external monitor A, B
    s.unplug(A)
    s.unplug(B)
    # The user plugs a headphone C
    s.plug(C)
    # The user selects headphone C as the activated device
    s.select(C)
    # The user unplugs headphone C
    s.unplug(C)
    # The user plugs a external monitor B => the activated device should be B
    s.plug(B)
    s.expect(B)
    # The user plugs a external monitor A => the activated device should be A
    s.plug(A)
    s.expect(A)
    # The user unplugs the external monitor A, B
    s.unplug(A)
    s.unplug(B)
    # The user plugs a external monitor A => the activated device should be A
    s.plug(A)
    s.expect(A)
    # The user plugs a external monitor B => the activated device should be A
    s.plug(B)
    s.expect(A)
    # The user unplugs the external monitor A, B
    s.unplug(A)
    s.unplug(B)
    # The user plugs a headphone C => the activated device should be C
    s.plug(C)
    # The user unplugs headphone C
    s.unplug(C)

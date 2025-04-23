# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.Headphone, T.HDMI)

    # internal speaker implicit
    s.plug(A)

    # The user plugs a headphone B
    s.plug(B)
    # The user selects headphone B as the activated device
    s.select(B)
    # The user unplugs headphone B
    s.unplug(B)
    # The user plugs a external monitor C
    s.plug(C)
    # The user selects the internal speaker A as the activated device
    s.select(A)
    # The user unplugs the external monitor C
    s.unplug(C)
    # The user plugs the headphone B => the activated device should be headphone B
    s.plug(B)
    s.expect(B)
    # The user unplugs the headphone B => the activated device should be internal speaker A
    s.unplug(B)
    s.expect(A)
    # The user plugs the external monitor C => the activated device should be internal speaker A
    s.plug(C)
    s.expect(A)
    # The user unplugs the external monitor C => the activated device should be internal speaker A
    s.unplug(C)
    s.expect(A)

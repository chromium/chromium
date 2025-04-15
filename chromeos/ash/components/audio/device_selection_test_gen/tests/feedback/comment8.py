# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.Headphone)
    s.plug(A)

    # The user plugs a monitor.
    s.plug(B)

    # The user selects the internal speaker as the activate device.
    s.select(A)

    # The user plugs a headphone.
    s.plug(C)

    # The user unplugs a headphone. => The activated device should be the internal speaker.
    s.unplug(C)
    s.expect(A)

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T


def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.Headphone)
    s.plug(A)

    # Connect to HDMI monitor
    s.plug(B)

    # Set Internal speaker as output
    s.select(A)

    # Disconnect HDMI
    s.unplug(B)

    # Connect to 3.5mm headset
    s.plug(C)

    # Set 3.5mm headset as output
    s.select(C)

    # Disconnect 3.5mm headset
    s.unplug(C)

    # Connect to HDMI monitor
    s.plug(B)

    # Output from internal speaker
    s.expect(A)

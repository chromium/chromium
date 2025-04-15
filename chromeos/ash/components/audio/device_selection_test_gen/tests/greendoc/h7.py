# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.Headphone)
    s.plug(A)

    # Connect HDMI monitor
    s.plug(B)

    # Choose Internal Speaker as output
    s.select(A)

    # Connect 3.5 mm Headphone or bluetooth device
    s.plug(C)

    # Disconnect Headphone
    s.unplug(C)

    # Output from internal speaker
    s.expect(A)

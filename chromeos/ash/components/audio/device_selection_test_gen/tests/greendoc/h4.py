# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B = s.abc(T.Internal, T.HDMI)
    s.plug(A)

    # Connect the HDMI
    s.plug(B)
    # Choose Internal Speaker
    s.select(A)
    # Dis-connect the HDMI
    s.unplug(B)
    # Connect the HDMI
    s.plug(B)
    # Output from internal speaker
    s.expect(A)

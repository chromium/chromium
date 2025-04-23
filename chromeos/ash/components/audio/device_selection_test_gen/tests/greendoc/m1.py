# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.HDMI)
    s.plug(A)

    # Connect a HDMI monitor1
    s.plug(B)

    # Connect a HDMI monitor2
    s.plug(C)

    # Output from HDMI monitor2
    s.expect(C)

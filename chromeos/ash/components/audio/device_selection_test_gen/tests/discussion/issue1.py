# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.USB, T.HDMI)
    s.plug(A)

    # User plugged 3.5mm headphone
    s.plug(B)
    # User connected HDMI monitor
    s.plug(C)
    # User selected HDMI monitor as the output
    s.select(C)
    # User selected 3.5mm headphone as the output
    s.select(B)
    # User selected internal speaker as the output
    s.select(A)
    # User unplugged 3.5mm headphone
    s.unplug(B)
    # User plugged 3.5mm headphone
    s.plug(B)

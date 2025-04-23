# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    A, B, C = s.abc(T.Internal, T.HDMI, T.USB)

    s.plug(A)

    # Actions in office:
    s.plug(C)
    s.select(C)
    s.unplug(C)

    # Actions at home:
    s.plug(B)
    s.select(A)
    s.unplug(B)

    # Actions in office:
    s.plug(C)
    s.expect(C)

    # Actions at home:
    s.plug(B)
    s.expect(A)

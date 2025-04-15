# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    I, A, B, C = s.abc(T.Internal, T.HDMI, T.USB, T.Headphone, charset='IABC')
    s.plug(I)

    s.plug(A)
    s.plug(B)
    s.select(A)
    s.plug(C)
    s.unplug(C)
    s.expect(A)

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import Simulator, T

def run(s: Simulator):
    I, A, B = s.abc(T.Internal, T.USB, T.USB, charset='IAB')
    s.plug(I)

    s.plug(A)
    s.plug(B)
    s.select(A)
    s.unplug(A)
    s.unplug(B)
    s.plug(B)
    s.plug(A)
    s.expect(A)

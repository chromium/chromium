# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from dsp import T, Simulator


def run(s: Simulator):
    A, B, C = s.abc(T.USB, T.USB, T.USB)

    s.plug(A)

    s.plug(B)
    s.select(B)

    s.plug(C)
    s.select(A)

    s.unplug(C)

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import org.chromium.components.payments.InputProtector;

/** An InputProtector.Clock implementation used by tests to simulate advancing the time. */
public class FakeClock implements InputProtector.Clock {
    private long mCurrentTimeMillis;

    public FakeClock() {
        // A reasonable fake time for testing, the value of System::currentTimeMillis() at the
        // time of writing this class.
        mCurrentTimeMillis = 1677609040000L;
    }

    @Override
    public long currentTimeMillis() {
        return mCurrentTimeMillis;
    }

    public void advanceCurrentTimeMillis(long millis) {
        mCurrentTimeMillis += millis;
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;

/** Uses touch gesture-based heuristics to detect use frustration. */
public class PlayerUserFrustrationDetector {
    private Runnable mFrustrationDetectionCallback;
    private List<Long> mTapsTimeMs = new ArrayList<>();

    static final int CONSECUTIVE_SINGLE_TAP_WINDOW_MS = 2 * 1000;
    static final int CONSECUTIVE_SINGLE_TAP_COUNT = 3;

    public PlayerUserFrustrationDetector(Runnable frustrationDetectionCallback) {
        mFrustrationDetectionCallback = frustrationDetectionCallback;
    }

    void recordUnconsumedTap() {
        recordUnconsumedTap(System.currentTimeMillis());
    }

    @VisibleForTesting
    void recordUnconsumedTap(long timeMs) {
        mTapsTimeMs.add(timeMs);

        for (int i = mTapsTimeMs.size() - 1; i > 0; --i) {
            if (mTapsTimeMs.get(i) - mTapsTimeMs.get(i - 1) > CONSECUTIVE_SINGLE_TAP_WINDOW_MS) {
                mTapsTimeMs.subList(0, i).clear();
                break;
            }
        }

        if (mTapsTimeMs.size() == CONSECUTIVE_SINGLE_TAP_COUNT) {
            mFrustrationDetectionCallback.run();
            mTapsTimeMs.clear();
        }
    }

    void recordUnconsumedLongPress() {
        mFrustrationDetectionCallback.run();
    }
}

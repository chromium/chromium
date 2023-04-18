// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is being moved to ./feed/
 */
@Deprecated
public interface FeedUserInteractionReliabilityLogger
        extends org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger {
    @IntDef({ClosedReason.OPEN_CARD, ClosedReason.SUSPEND_APP, ClosedReason.LEAVE_FEED,
            ClosedReason.SWITCH_STREAM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClosedReason {
        int OPEN_CARD = 0;
        int SUSPEND_APP = 1;
        int LEAVE_FEED = 2;
        int SWITCH_STREAM = 3;
    }
    @IntDef({PaginationResult.SUCCESS_WITH_MORE_FEED, PaginationResult.SUCCESS_WITH_NO_FEED,
            PaginationResult.FAILURE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PaginationResult {
        int SUCCESS_WITH_MORE_FEED = 0;
        int SUCCESS_WITH_NO_FEED = 1;
        int FAILURE = 2;
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is being moved to ./feed/
 */
public interface FeedLaunchReliabilityLogger
        extends org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger {
    /** Type of surface the feed is being launched on. */
    @IntDef({SurfaceType.UNSPECIFIED, SurfaceType.NEW_TAB_PAGE, SurfaceType.START_SURFACE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SurfaceType {
        int UNSPECIFIED = 0;
        int NEW_TAB_PAGE = 1;
        int START_SURFACE = 2;
    }

    /** @deprecated: Please use org.chromium.chrome.browser.xsurface.feed.StreamType */
    @Deprecated
    @IntDef({StreamType.UNSPECIFIED, StreamType.FOR_YOU, StreamType.WEB_FEED,
            StreamType.SINGLE_WEB_FEED})
    @Retention(RetentionPolicy.SOURCE)
    @interface StreamType {
        int UNSPECIFIED = 0;
        int FOR_YOU = 1;
        int WEB_FEED = 2;
        int SINGLE_WEB_FEED = 3;
    }
}

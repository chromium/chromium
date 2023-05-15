// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This is being moved to ./feed/ */
@Deprecated
public interface FeedActionsHandler
        extends org.chromium.chrome.browser.xsurface.feed.FeedActionsHandler {
    public enum SnackbarDuration { SHORT, LONG }

    @IntDef({FeedIdentifier.UNSPECIFIED, FeedIdentifier.MAIN_FEED, FeedIdentifier.FOLLOWING_FEED,
            FeedIdentifier.CHANNEL_FEED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeedIdentifier {
        int UNSPECIFIED = 0;
        int MAIN_FEED = 1;
        int FOLLOWING_FEED = 2;
        int CHANNEL_FEED = 3;
    }
    /** This is being moved to ./feed/ */
    public interface SnackbarController extends org.chromium.chrome.browser.xsurface.feed
                                                        .FeedActionsHandler.SnackbarController {}

    default void showSnackbar(String text, String actionLabel, SnackbarDuration duration,
            SnackbarController controller) {}
}

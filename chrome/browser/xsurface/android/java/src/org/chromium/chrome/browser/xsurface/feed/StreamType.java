// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Type of stream being launched (the "For you","Following", "SingleWebFeed", or "Supervised User"
 * feed).
 */
@IntDef({
    StreamType.UNSPECIFIED,
    StreamType.FOR_YOU,
    StreamType.WEB_FEED,
    StreamType.SINGLE_WEB_FEED,
    StreamType.SUPERVISED_USER_FEED
})
@Retention(RetentionPolicy.SOURCE)
public @interface StreamType {
    int UNSPECIFIED = 0;
    int FOR_YOU = 1;
    int WEB_FEED = 2;
    int SINGLE_WEB_FEED = 3;
    int SUPERVISED_USER_FEED = 4;
}

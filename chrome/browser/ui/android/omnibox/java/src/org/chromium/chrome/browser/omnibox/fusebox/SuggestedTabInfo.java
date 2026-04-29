// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** Contains information needed to render a suggested tab chip in the UI. */
@NullMarked
@JNINamespace("contextual_tasks")
public class SuggestedTabInfo {
    public final int tabId;
    public final String title;
    public final GURL url;
    public final long lastActiveTime;

    @CalledByNative
    public SuggestedTabInfo(int tabId, String title, GURL url, long lastActiveTime) {
        this.tabId = tabId;
        this.title = title;
        this.url = url;
        this.lastActiveTime = lastActiveTime;
    }
}

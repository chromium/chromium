// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

/**
 * Represents the state of a toolbar action for a particular tab.
 *
 * <p>This object is returned by {@link ToolbarActionBridge}.
 */
public class ToolbarAction {
    @NonNull private final String mId;
    @NonNull private final String mTitle;

    @CalledByNative
    private ToolbarAction(@JniType("std::string") String id, @JniType("std::string") String title) {
        mId = id;
        mTitle = title;
    }

    @NonNull
    public String getId() {
        return mId;
    }

    @NonNull
    public String getTitle() {
        return mTitle;
    }
}

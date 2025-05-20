// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents the state of an extension action for a particular tab.
 *
 * <p>This object is returned by {@link ExtensionActionBridge}.
 */
@NullMarked
public class ExtensionAction {
    private final String mId;
    private final String mTitle;

    @CalledByNative
    @VisibleForTesting
    ExtensionAction(@JniType("std::string") String id, @JniType("std::string") String title) {
        mId = id;
        mTitle = title;
    }

    public String getId() {
        return mId;
    }

    public String getTitle() {
        return mTitle;
    }
}

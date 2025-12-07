// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.Objects;

/**
 * Represents the state of an extension action for a particular tab.
 *
 * <p>This object is returned by {@link ExtensionActionBridge}.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionAction {
    private final String mId;
    private final String mTitle;

    @CalledByNative
    @VisibleForTesting
    public ExtensionAction(
            @JniType("std::string") String id, @JniType("std::string") String title) {
        mId = id;
        mTitle = title;
    }

    public String getId() {
        return mId;
    }

    public String getTitle() {
        return mTitle;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof ExtensionAction other) {
            return mId.equals(other.mId) && mTitle.equals(other.mTitle);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mId, mTitle);
    }
}

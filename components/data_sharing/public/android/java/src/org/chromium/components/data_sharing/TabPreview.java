// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.url.GURL;

/** Preview about a shared tab group. */
@JNINamespace("data_sharing")
public class TabPreview {
    public final GURL url;
    public final String displayUrl;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TabPreview(GURL url, String displayUrl) {
        this.url = url;
        this.displayUrl = displayUrl;
    }

    @CalledByNative
    private static TabPreview createTabPreview(GURL gurl, String displayUrl) {
        return new TabPreview(gurl, displayUrl);
    }
}

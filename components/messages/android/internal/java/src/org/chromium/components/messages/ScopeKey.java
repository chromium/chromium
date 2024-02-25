// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** A key stands for a scope instance of a certain scope type. */
class ScopeKey {
    public final @MessageScopeType int scopeType;
    public final @Nullable WebContents webContents;
    public final @Nullable WindowAndroid windowAndroid;

    public ScopeKey(@MessageScopeType int scopeType, @NonNull WebContents webContents) {
        this.scopeType = scopeType;
        this.webContents = webContents;
        windowAndroid = null;
    }

    public ScopeKey(@NonNull WindowAndroid windowAndroid) {
        scopeType = MessageScopeType.WINDOW;
        this.windowAndroid = windowAndroid;
        webContents = null;
    }

    @Override
    public boolean equals(@Nullable Object other) {
        if (!(other instanceof ScopeKey)) return false;
        ScopeKey otherScopeKey = (ScopeKey) other;
        return scopeType == otherScopeKey.scopeType
                && windowAndroid == otherScopeKey.windowAndroid
                && webContents == otherScopeKey.webContents;
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = result * 31 + scopeType;
        result = result * 31 + (webContents == null ? 0 : webContents.hashCode());
        result = result * 31 + (windowAndroid == null ? 0 : windowAndroid.hashCode());
        return result;
    }
}

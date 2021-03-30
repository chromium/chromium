// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * A key stands for a scope instance of a certain scope type.
 */
class ScopeKey {
    public final @MessageScopeType int scopeType;
    public final WebContents webContents;

    public ScopeKey(@MessageScopeType int scopeType, WebContents webContents) {
        this.scopeType = scopeType;
        this.webContents = webContents;
    }

    @Override
    public boolean equals(@Nullable Object other) {
        if (!(other instanceof ScopeKey)) return false;
        ScopeKey otherScopeKey = (ScopeKey) other;
        return scopeType == otherScopeKey.scopeType && otherScopeKey.webContents == webContents;
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = result * 31 + scopeType;
        result = result * 31 + webContents.hashCode();
        return result;
    }
}

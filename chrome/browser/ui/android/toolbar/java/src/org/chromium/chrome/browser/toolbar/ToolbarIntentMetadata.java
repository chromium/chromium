// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

/** Simple collection of information about the original intent. */
public class ToolbarIntentMetadata {
    private final boolean mIsMainIntentFromLauncher;
    private final boolean mIsIntentWithEffect;

    /**
     * @param isMainIntentFromLauncher Whether the intent was the result of launching from the app
     *         icon.
     * @param isIntentWithEffect If the intent caused a special action or effect to apply on launch.
     */
    public ToolbarIntentMetadata(boolean isMainIntentFromLauncher, boolean isIntentWithEffect) {
        mIsMainIntentFromLauncher = isMainIntentFromLauncher;
        mIsIntentWithEffect = isIntentWithEffect;
    }

    /** @return Whether the intent was the result of launching from the app icon. */
    public boolean getIsMainIntentFromLauncher() {
        return mIsMainIntentFromLauncher;
    }

    /** @return If the intent caused a special action or effect to apply on launch. */
    public boolean getIsIntentWithEffect() {
        return mIsIntentWithEffect;
    }
}

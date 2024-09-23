// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

/** Data class defining UI overrides for the {@link LocationBar}. */
public class LocationBarEmbedderUiOverrides {
    private boolean mForcedPhoneStyleOmnibox;
    private boolean mLensEntrypointAllowed;
    private boolean mVoiceEntrypointAllowed;

    public LocationBarEmbedderUiOverrides() {
        mLensEntrypointAllowed = true;
        mVoiceEntrypointAllowed = true;
    }

    /**
     * Whether a "phone-style" (full bleed, unrounded corners) omnibox suggestions list should be
     * used even when the screen width is >600dp.
     */
    public boolean isForcedPhoneStyleOmnibox() {
        return mForcedPhoneStyleOmnibox;
    }

    /**
     * Force a "phone-style" (full bleed, unrounded corners) omnibox suggestions list to be used
     * even when the screen width is >600dp.
     *
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setForcedPhoneStyleOmnibox() {
        mForcedPhoneStyleOmnibox = true;
        return this;
    }

    /** Whether Lens entrypoint should be offered to the user. */
    public boolean isLensEntrypointAllowed() {
        return mLensEntrypointAllowed;
    }

    /**
     * Specify whether Lens entrypoint should be offered to the user.
     *
     * @param isAllowed whether Lens entrypoint should be shown in the Location bar
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setLensEntrypointAllowed(boolean isAllowed) {
        mLensEntrypointAllowed = isAllowed;
        return this;
    }

    /** Whether Voice entrypoint should be offered to the user. */
    public boolean isVoiceEntrypointAllowed() {
        return mVoiceEntrypointAllowed;
    }

    /**
     * Specify whether Voice entrypoint should be offered to the user.
     *
     * @param isAllowed whether Voice entrypoint should be shown in the Location bar
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setVoiceEntrypointAllowed(boolean isAllowed) {
        mVoiceEntrypointAllowed = isAllowed;
        return this;
    }
}

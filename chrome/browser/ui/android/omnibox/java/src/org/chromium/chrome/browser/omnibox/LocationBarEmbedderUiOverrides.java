// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

/** Data class defining UI overrides for the {@link LocationBar}. */
public class LocationBarEmbedderUiOverrides {
    private boolean mForcedPhoneStyleOmnibox;

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
}

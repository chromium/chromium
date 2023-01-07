// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.url.GURL;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public class OmniboxPedal {
    private final @OmniboxPedalType int mId;
    private final @NonNull String mHint;
    private final @NonNull String mSuggestionContents;
    private final @NonNull String mAccessibilitySuffix;
    private final @NonNull String mAccessibilityHint;
    private final @Nullable GURL mUrl;

    public OmniboxPedal(@OmniboxPedalType int id, @NonNull String hint,
            @NonNull String suggestionContents, @NonNull String accessibilitySuffix,
            @NonNull String accessibilityHint, @Nullable GURL url) {
        mId = id;
        mHint = hint;
        mSuggestionContents = suggestionContents;
        mAccessibilitySuffix = accessibilitySuffix;
        mAccessibilityHint = accessibilityHint;
        mUrl = url;
    }

    public boolean hasPedalId() {
        return (mId >= OmniboxPedalType.NONE) && mId < (OmniboxPedalType.TOTAL_COUNT);
    }

    public boolean hasActionId() {
        return (mId >= OmniboxActionType.FIRST) && mId < (OmniboxActionType.LAST);
    }

    /**
     * @return an ID used to identify the underlying pedal.
     */
    public @OmniboxPedalType int getPedalID() {
        assert hasPedalId();
        return mId;
    }

    /**
     * @return an ID used to identify the underlying action.
     */
    public @OmniboxActionType int getActionID() {
        assert hasActionId();
        return mId;
    }

    /**
     * @return the hint for the action.
     */
    public @NonNull String getHint() {
        return mHint;
    }

    /**
     * @return the suggestion contents for the action.
     */
    public @NonNull String getSuggestionContents() {
        return mSuggestionContents;
    }

    /**
     * @return the accessibility suffix for the action.
     */
    public @NonNull String getAccessibilitySuffix() {
        return mAccessibilitySuffix;
    }

    /**
     * @return the accessibility hint for the action.
     */
    public @NonNull String getAccessibilityHint() {
        return mAccessibilityHint;
    }

    /**
     * @return the URL for the action.
     */
    public @Nullable GURL getUrl() {
        return mUrl;
    }

    @CalledByNative
    private static OmniboxPedal build(int id, @NonNull String hint,
            @NonNull String suggestionContents, @NonNull String accessibilitySuffix,
            @NonNull String accessibilityHint, @Nullable GURL url) {
        return new OmniboxPedal(
                id, hint, suggestionContents, accessibilitySuffix, accessibilityHint, url);
    }
}
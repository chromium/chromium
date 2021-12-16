// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.GURL;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public class OmniboxPedal {
    private final int mId;
    private final @NonNull String mHint;
    private final @NonNull String mSuggestionContents;
    private final @NonNull String mAccessibilitySuffix;
    private final @NonNull String mAccessibilityHint;
    private final @Nullable GURL mUrl;

    public OmniboxPedal(int id, @NonNull String hint, @NonNull String suggestionContents,
            @NonNull String accessibilitySuffix, @NonNull String accessibilityHint,
            @Nullable GURL url) {
        mId = id;
        mHint = hint;
        mSuggestionContents = suggestionContents;
        mAccessibilitySuffix = accessibilitySuffix;
        mAccessibilityHint = accessibilityHint;
        mUrl = url;
    }

    /**
     * @return an ID used to identify some actions. Not defined for all Actions.
     */
    public int getID() {
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
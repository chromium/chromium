// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.Objects;

/** Data-only class representing SearchActivity preferences. */
@NullMarked
public final class SearchActivityPreferences {
    /** The default/fallback value describing Voice Search availability. */
    /* package */ static final boolean DEFAULT_VOICE_SEARCH_AVAILABILITY = true;

    /** The default/fallback value describing Google Lens availability. */
    /* package */ static final boolean DEFAULT_GOOGLE_LENS_AVAILABILITY = false;

    /** The default/fallback value describing Incognito browsing availability. */
    /* package */ static final boolean DEFAULT_INCOGNITO_AVAILABILITY = true;

    /** The default/fallback value describing AI Mode availability. */
    /* package */ static final boolean DEFAULT_AI_MODE_AVAILABILITY = false;

    /** Signed-in account email. */
    public final @Nullable String accountEmail;

    /** Name of the Default Search Engine. */
    public final @Nullable String searchEngineName;

    /** URL of the Default Search Engine. */
    public final GURL searchEngineUrl;

    /** Whether Voice Search functionality is available. */
    public final boolean voiceSearchAvailable;

    /** Whether Google Lens functionality is available. */
    public final boolean googleLensAvailable;

    /** Whether Incognito browsing functionality is available. */
    public final boolean incognitoAvailable;

    /** Whether AI Mode functionality is available. */
    public final boolean aiModeAvailable;

    private SearchActivityPreferences(Builder builder) {
        this.accountEmail = builder.mAccountEmail;
        this.searchEngineName = builder.mSearchEngineName;
        this.searchEngineUrl =
                builder.mSearchEngineUrl != null ? builder.mSearchEngineUrl : GURL.emptyGURL();
        this.voiceSearchAvailable = builder.mVoiceSearchAvailable;
        this.googleLensAvailable = builder.mGoogleLensAvailable;
        this.incognitoAvailable = builder.mIncognitoAvailable;
        this.aiModeAvailable = builder.mAiModeAvailable;
    }

    /** Creates a new {@link Builder} initialized with this object's values. */
    public Builder toBuilder() {
        return new Builder(this);
    }

    /** Builder for {@link SearchActivityPreferences}. */
    public static final class Builder {
        private @Nullable String mAccountEmail;
        private @Nullable String mSearchEngineName;
        private @Nullable GURL mSearchEngineUrl;
        private boolean mVoiceSearchAvailable = DEFAULT_VOICE_SEARCH_AVAILABILITY;
        private boolean mGoogleLensAvailable = DEFAULT_GOOGLE_LENS_AVAILABILITY;
        private boolean mIncognitoAvailable = DEFAULT_INCOGNITO_AVAILABILITY;
        private boolean mAiModeAvailable = DEFAULT_AI_MODE_AVAILABILITY;

        public Builder() {}

        private Builder(SearchActivityPreferences copyFrom) {
            mAccountEmail = copyFrom.accountEmail;
            mSearchEngineName = copyFrom.searchEngineName;
            mSearchEngineUrl = copyFrom.searchEngineUrl;
            mVoiceSearchAvailable = copyFrom.voiceSearchAvailable;
            mGoogleLensAvailable = copyFrom.googleLensAvailable;
            mIncognitoAvailable = copyFrom.incognitoAvailable;
            mAiModeAvailable = copyFrom.aiModeAvailable;
        }

        public Builder setAccountEmail(@Nullable String accountEmail) {
            mAccountEmail = accountEmail;
            return this;
        }

        public Builder setSearchEngineName(@Nullable String searchEngineName) {
            mSearchEngineName = searchEngineName;
            return this;
        }

        public Builder setSearchEngineUrl(@Nullable GURL searchEngineUrl) {
            mSearchEngineUrl = searchEngineUrl;
            return this;
        }

        public Builder setVoiceSearchAvailable(boolean voiceSearchAvailable) {
            mVoiceSearchAvailable = voiceSearchAvailable;
            return this;
        }

        public Builder setGoogleLensAvailable(boolean googleLensAvailable) {
            mGoogleLensAvailable = googleLensAvailable;
            return this;
        }

        public Builder setIncognitoAvailable(boolean incognitoAvailable) {
            mIncognitoAvailable = incognitoAvailable;
            return this;
        }

        public Builder setAiModeAvailable(boolean aiModeAvailable) {
            mAiModeAvailable = aiModeAvailable;
            return this;
        }

        public SearchActivityPreferences build() {
            return new SearchActivityPreferences(this);
        }
    }

    @Override
    public boolean equals(Object otherObj) {
        if (otherObj == this) return true;
        if (!(otherObj instanceof SearchActivityPreferences)) return false;

        SearchActivityPreferences other = (SearchActivityPreferences) otherObj;
        return voiceSearchAvailable == other.voiceSearchAvailable
                && googleLensAvailable == other.googleLensAvailable
                && incognitoAvailable == other.incognitoAvailable
                && aiModeAvailable == other.aiModeAvailable
                && TextUtils.equals(searchEngineName, other.searchEngineName)
                && searchEngineUrl.equals(other.searchEngineUrl)
                && TextUtils.equals(accountEmail, other.accountEmail);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                searchEngineName,
                searchEngineUrl,
                voiceSearchAvailable,
                googleLensAvailable,
                incognitoAvailable,
                aiModeAvailable,
                accountEmail);
    }
}

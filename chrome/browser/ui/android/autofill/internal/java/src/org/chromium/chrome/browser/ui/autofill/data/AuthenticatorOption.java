// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.data;

import org.chromium.chrome.browser.ui.autofill.CardUnmaskChallengeOptionType;

/**
 * Represents an authenticator option to be displayed in the {@link AuthenticatorSelectionDialog}.
 */
public class AuthenticatorOption {
    private final String mTitle;
    private final String mIdentifier;
    private final String mDescription;
    private final int mIconResId;
    private final @CardUnmaskChallengeOptionType int mType;

    private AuthenticatorOption(
            String title,
            String identifier,
            String description,
            int iconResId,
            @CardUnmaskChallengeOptionType int type) {
        this.mTitle = title;
        this.mIdentifier = identifier;
        this.mDescription = description;
        this.mIconResId = iconResId;
        this.mType = type;
    }

    /**
     * Returns the title of the authenticator option. This describes the type of the
     * authenticator.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * Returns the identifier of the authenticator option that should be passed to the backend on
     * selection.
     */
    public String getIdentifier() {
        return mIdentifier;
    }

    /** Returns the description of the authenticator option. */
    public String getDescription() {
        return mDescription;
    }

    /** Returns the resource id of the authenticator option's icon. */
    public int getIconResId() {
        return mIconResId;
    }

    /** Returns the type of the authenticator option. */
    public int getType() {
        return mType;
    }

    /** Builder for {@link AuthenticatorOption}. */
    public static final class Builder {
        private String mTitle;
        private String mIdentifier;
        private String mDescription;
        private int mIconResId;
        private @CardUnmaskChallengeOptionType int mType;

        public Builder setTitle(String title) {
            this.mTitle = title;
            return this;
        }

        public Builder setIdentifier(String identifier) {
            this.mIdentifier = identifier;
            return this;
        }

        public Builder setDescription(String description) {
            this.mDescription = description;
            return this;
        }

        public Builder setIconResId(int iconResId) {
            this.mIconResId = iconResId;
            return this;
        }

        public Builder setType(@CardUnmaskChallengeOptionType int type) {
            this.mType = type;
            return this;
        }

        public AuthenticatorOption build() {
            assert mTitle != null && !mTitle.isEmpty()
                    : "title for the AuthenticatorOption must be set";
            assert mIdentifier != null && !mIdentifier.isEmpty()
                    : "identifier for the AuthenticatorOption must be set";
            assert mDescription != null && !mDescription.isEmpty()
                    : "description for the AuthenticatorOption must be set";
            assert mType != 0 : "type for the AuthenticatorOption must be set";
            return new AuthenticatorOption(mTitle, mIdentifier, mDescription, mIconResId, mType);
        }
    }
}

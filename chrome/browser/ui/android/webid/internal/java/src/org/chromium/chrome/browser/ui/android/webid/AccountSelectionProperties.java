// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties defined here reflect the state of the AccountSelection-components.
 */
class AccountSelectionProperties {
    /**
     * Properties for an account entry in AccountSelection sheet.
     */
    static class AccountProperties {
        static class Avatar {
            // Name is used to create a fallback monogram Icon.
            final String mName;
            final Bitmap mAvatar;
            final int mAvatarSize;

            Avatar(String name, @Nullable Bitmap avatar, int avatarSize) {
                mName = name;
                mAvatar = avatar;
                mAvatarSize = avatarSize;
            }
        }

        static class FaviconOrFallback {
            final GURL mUrl;
            final @Nullable Bitmap mIcon;
            final int mFallbackColor;
            final int mIconSize;

            FaviconOrFallback(
                    GURL originUrl, @Nullable Bitmap icon, int fallbackColor, int iconSize) {
                mUrl = originUrl;
                mIcon = icon;
                mFallbackColor = fallbackColor;
                mIconSize = iconSize;
            }
        }

        static final WritableObjectPropertyKey<Avatar> AVATAR =
                new WritableObjectPropertyKey<>("avatar");
        static final WritableObjectPropertyKey<FaviconOrFallback> FAVICON_OR_FALLBACK =
                new WritableObjectPropertyKey<>("favicon");
        static final ReadableObjectPropertyKey<Account> ACCOUNT =
                new ReadableObjectPropertyKey<>("account");
        static final ReadableObjectPropertyKey<Callback<Account>> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {
                AVATAR, FAVICON_OR_FALLBACK, ACCOUNT, ON_CLICK_LISTENER};

        private AccountProperties() {}
    }

    /**
     * Properties defined here reflect the state of the header in the AccountSelection
     * sheet.
     */
    static class HeaderProperties {
        public enum HeaderType { SINGLE_ACCOUNT, MULTIPLE_ACCOUNT, SIGN_IN, VERIFY }
        static final ReadableObjectPropertyKey<Runnable> CLOSE_ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("close_on_click_listener");
        static final ReadableObjectPropertyKey<String> FORMATTED_IDP_URL =
                new ReadableObjectPropertyKey<>("formatted_idp_url");
        static final ReadableObjectPropertyKey<String> FORMATTED_RP_URL =
                new ReadableObjectPropertyKey<>("formatted_rp_url");
        static final ReadableObjectPropertyKey<HeaderType> TYPE =
                new ReadableObjectPropertyKey<>("type");

        static final PropertyKey[] ALL_KEYS = {
                CLOSE_ON_CLICK_LISTENER, FORMATTED_IDP_URL, FORMATTED_RP_URL, TYPE};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the state of the continue button in the AccountSelection
     * sheet.
     */
    static class DataSharingConsentProperties {
        static class Properties {
            public String mFormattedIdpUrl;
            public String mFormattedRpUrl;
            public String mTermsOfServiceUrl;
            public String mPrivacyPolicyUrl;
        }

        static final ReadableObjectPropertyKey<Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private DataSharingConsentProperties() {}
    }

    /**
     * Properties defined here reflect the state of the continue button in the AccountSelection
     * sheet.
     */
    static class ContinueButtonProperties {
        static final ReadableObjectPropertyKey<Account> ACCOUNT =
                new ReadableObjectPropertyKey<>("account");
        static final ReadableObjectPropertyKey<IdentityProviderMetadata> IDP_METADATA =
                new ReadableObjectPropertyKey<>("idp_metadata");
        static final ReadableObjectPropertyKey<Callback<Account>> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {ACCOUNT, IDP_METADATA, ON_CLICK_LISTENER};

        private ContinueButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of the cancel button used for auto sign in.
     */
    static class AutoSignInCancelButtonProperties {
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK_LISTENER};

        private AutoSignInCancelButtonProperties() {}
    }

    @IntDef({ItemType.HEADER, ItemType.ACCOUNT, ItemType.CONTINUE_BUTTON,
            ItemType.AUTO_SIGN_IN_CANCEL_BUTTON, ItemType.DATA_SHARING_CONSENT})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /**
         * The header at the top of the accounts sheet.
         */
        int HEADER = 1;

        /**
         * A section containing a user's name and email.
         */
        int ACCOUNT = 2;

        /**
         * The continue button at the end of the sheet when there is only one account.
         */
        int CONTINUE_BUTTON = 3;

        /**
         * The cancel button at the end of the sheet with auto sign in.
         */
        int AUTO_SIGN_IN_CANCEL_BUTTON = 4;

        /**
         * The user data sharing consent text when there is only one account and it is a sign-up
         * moment.
         */
        int DATA_SHARING_CONSENT = 5;
    }

    private AccountSelectionProperties() {}
}

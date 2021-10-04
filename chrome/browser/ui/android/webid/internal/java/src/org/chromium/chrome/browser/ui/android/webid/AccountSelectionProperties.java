// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
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

        static final PropertyModel.WritableObjectPropertyKey<Avatar> AVATAR =
                new PropertyModel.WritableObjectPropertyKey<>("avatar");
        static final PropertyModel
                .WritableObjectPropertyKey<FaviconOrFallback> FAVICON_OR_FALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>("favicon");
        static final PropertyModel.ReadableObjectPropertyKey<Account> ACCOUNT =
                new PropertyModel.ReadableObjectPropertyKey<>("account");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<Account>> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {
                AVATAR, FAVICON_OR_FALLBACK, ACCOUNT, ON_CLICK_LISTENER};

        private AccountProperties() {}
    }

    /**
     * Properties defined here reflect the state of the header in the AccountSelection
     * sheet.
     */
    static class HeaderProperties {
        public enum HeaderType { SINGLE_ACCOUNT, MULTIPLE_ACCOUNT, SIGN_IN }
        static final PropertyModel.ReadableObjectPropertyKey<HeaderType> TYPE =
                new PropertyModel.ReadableObjectPropertyKey<>("type");
        static final PropertyModel.ReadableObjectPropertyKey<String> FORMATTED_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("formatted_url");

        static final PropertyKey[] ALL_KEYS = {TYPE, FORMATTED_URL};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the state of the continue button in the AccountSelection
     * sheet.
     */
    static class DataSharingConsentProperties {
        static final PropertyModel.ReadableObjectPropertyKey<String> PROVIDER_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("provider_url");
        static final PropertyModel.ReadableObjectPropertyKey<String> TERMS_OF_SERVICE_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("terms_of_service_url");
        static final PropertyModel.ReadableObjectPropertyKey<String> PRIVACY_POLICY_URL =
                new PropertyModel.ReadableObjectPropertyKey<>("privacy_policy_url");

        static final PropertyKey[] ALL_KEYS = {
                PROVIDER_URL, TERMS_OF_SERVICE_URL, PRIVACY_POLICY_URL};

        private DataSharingConsentProperties() {}
    }

    /**
     * Properties defined here reflect the state of the continue button in the AccountSelection
     * sheet.
     */
    static class ContinueButtonProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Account> ACCOUNT =
                new PropertyModel.ReadableObjectPropertyKey<>("account");
        static final PropertyModel.ReadableObjectPropertyKey<Callback<Account>> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {ACCOUNT, ON_CLICK_LISTENER};

        private ContinueButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of the cancel button used for auto sign in.
     */
    static class AutoSignInCancelButtonProperties {
        static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
                new PropertyModel.ReadableObjectPropertyKey<>("on_click_listener");

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

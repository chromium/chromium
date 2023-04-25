// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

/**
 * Properties defined here reflect the state of the AccountSelection-components.
 */
class AccountSelectionProperties {
    public static final int ITEM_TYPE_ACCOUNT = 1;

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

        static final WritableObjectPropertyKey<Avatar> AVATAR =
                new WritableObjectPropertyKey<>("avatar");
        static final ReadableObjectPropertyKey<Account> ACCOUNT =
                new ReadableObjectPropertyKey<>("account");
        static final ReadableObjectPropertyKey<Callback<Account>> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {AVATAR, ACCOUNT, ON_CLICK_LISTENER};

        private AccountProperties() {}
    }

    /**
     * Properties defined here reflect the state of the header in the AccountSelection
     * sheet.
     */
    static class HeaderProperties {
        public enum HeaderType { SIGN_IN, VERIFY, VERIFY_AUTO_REAUTHN }
        static final ReadableObjectPropertyKey<Runnable> CLOSE_ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("close_on_click_listener");
        static final ReadableObjectPropertyKey<String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");
        static final ReadableObjectPropertyKey<String> TOP_FRAME_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("top_frame_for_display");
        static final ReadableObjectPropertyKey<String> IFRAME_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("iframe_for_display");
        static final ReadableObjectPropertyKey<Bitmap> IDP_BRAND_ICON =
                new ReadableObjectPropertyKey<>("brand_icon");
        static final ReadableObjectPropertyKey<HeaderType> TYPE =
                new ReadableObjectPropertyKey<>("type");
        static final ReadableObjectPropertyKey<String> RP_CONTEXT =
                new ReadableObjectPropertyKey<>("rp_context");

        static final PropertyKey[] ALL_KEYS = {CLOSE_ON_CLICK_LISTENER, IDP_FOR_DISPLAY,
                TOP_FRAME_FOR_DISPLAY, IFRAME_FOR_DISPLAY, IDP_BRAND_ICON, TYPE, RP_CONTEXT};

        private HeaderProperties() {}
    }

    /**
     * Properties defined here reflect the state of the continue button in the AccountSelection
     * sheet.
     */
    static class DataSharingConsentProperties {
        static class Properties {
            public String mIdpForDisplay;
            public GURL mTermsOfServiceUrl;
            public GURL mPrivacyPolicyUrl;
            public Runnable mTermsOfServiceClickRunnable;
            public Runnable mPrivacyPolicyClickRunnable;
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
     * Properties defined here reflect sections in the FedCM bottom sheet.
     */
    static class ItemProperties {
        static final WritableObjectPropertyKey<PropertyModel> CONTINUE_BUTTON =
                new WritableObjectPropertyKey<>("continue_btn");
        static final WritableObjectPropertyKey<PropertyModel> DATA_SHARING_CONSENT =
                new WritableObjectPropertyKey<>("data_sharing_consent");
        static final WritableObjectPropertyKey<PropertyModel> HEADER =
                new WritableObjectPropertyKey<>("header");

        static final PropertyKey[] ALL_KEYS = {CONTINUE_BUTTON, DATA_SHARING_CONSENT, HEADER};

        private ItemProperties() {}
    }

    private AccountSelectionProperties() {}
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.function.Consumer;

/** Properties defined here reflect the state of the AccountSelection-components. */
class AccountSelectionProperties {
    public static final int ITEM_TYPE_ACCOUNT = 1;
    public static final int ITEM_TYPE_ADD_ACCOUNT = 2;

    /** Properties for an account entry in AccountSelection sheet. */
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
        public enum HeaderType {
            SIGN_IN,
            VERIFY,
            VERIFY_AUTO_REAUTHN,
            SIGN_IN_TO_IDP_STATIC,
            SIGN_IN_ERROR,
            LOADING,
            REQUEST_PERMISSION
        }

        static final ReadableObjectPropertyKey<Runnable> CLOSE_ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("close_on_click_listener");
        static final ReadableObjectPropertyKey<String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");
        static final ReadableObjectPropertyKey<String> RP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("rp_for_display");
        static final ReadableObjectPropertyKey<Bitmap> IDP_BRAND_ICON =
                new ReadableObjectPropertyKey<>("idp_brand_icon");
        static final ReadableObjectPropertyKey<Bitmap> RP_BRAND_ICON =
                new ReadableObjectPropertyKey<>("rp_brand_icon");
        static final ReadableObjectPropertyKey<HeaderType> TYPE =
                new ReadableObjectPropertyKey<>("type");
        static final ReadableIntPropertyKey RP_CONTEXT = new ReadableIntPropertyKey("rp_context");
        static final ReadableObjectPropertyKey<Integer> RP_MODE =
                new ReadableObjectPropertyKey<>("rp_mode");
        static final ReadableBooleanPropertyKey IS_MULTIPLE_ACCOUNT_CHOOSER =
                new ReadableBooleanPropertyKey("is_multiple_account_chooser");
        static final ReadableObjectPropertyKey<Callback<View>> SET_FOCUS_VIEW_CALLBACK =
                new ReadableObjectPropertyKey<>("set_focus_view_callback");

        static final PropertyKey[] ALL_KEYS = {
            CLOSE_ON_CLICK_LISTENER,
            IDP_FOR_DISPLAY,
            RP_FOR_DISPLAY,
            IDP_BRAND_ICON,
            RP_BRAND_ICON,
            TYPE,
            RP_CONTEXT,
            RP_MODE,
            IS_MULTIPLE_ACCOUNT_CHOOSER,
            SET_FOCUS_VIEW_CALLBACK
        };

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
            public Consumer<Context> mTermsOfServiceClickCallback;
            public Consumer<Context> mPrivacyPolicyClickCallback;
            public Callback<View> mSetFocusViewCallback;
            public @IdentityRequestDialogDisclosureField int[] mDisclosureFields;
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
        static class Properties {
            public Account mAccount;
            public IdentityProviderMetadata mIdpMetadata;
            public Callback<Account> mOnClickListener;
            public HeaderProperties.HeaderType mHeaderType;
            public Callback<View> mSetFocusViewCallback;
        }

        static final ReadableObjectPropertyKey<Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private ContinueButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of the add account button in the AccountSelection
     * sheet.
     */
    static class AddAccountButtonProperties {
        static class Properties {
            public IdentityProviderMetadata mIdpMetadata;
            public Callback<Account> mOnClickListener;
        }

        static final ReadableObjectPropertyKey<Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private AddAccountButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of the got it button in the AccountSelection sheet.
     */
    static class ErrorButtonProperties {
        static final ReadableObjectPropertyKey<IdentityProviderMetadata> IDP_METADATA =
                new ReadableObjectPropertyKey<>("idp_metadata");
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {IDP_METADATA, ON_CLICK_LISTENER};

        private ErrorButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of the IDP sign in text in the AccountSelection
     * sheet.
     */
    static class IdpSignInProperties {
        static final ReadableObjectPropertyKey<String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");

        static final PropertyKey[] ALL_KEYS = {IDP_FOR_DISPLAY};

        private IdpSignInProperties() {}
    }

    /**
     * Properties defined here reflect the state of the error text in the AccountSelection
     * sheet.
     */
    static class ErrorProperties {
        static class Properties {
            public String mIdpForDisplay;
            public String mRpForDisplay;
            public IdentityCredentialTokenError mError;
            public Runnable mMoreDetailsClickRunnable;
        }

        static final ReadableObjectPropertyKey<Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private ErrorProperties() {}
    }

    /** Properties defined here reflect sections in the FedCM bottom sheet. */
    static class ItemProperties {
        static final WritableObjectPropertyKey<PropertyModel> CONTINUE_BUTTON =
                new WritableObjectPropertyKey<>("continue_btn");
        static final WritableObjectPropertyKey<PropertyModel> DATA_SHARING_CONSENT =
                new WritableObjectPropertyKey<>("data_sharing_consent");
        static final WritableObjectPropertyKey<PropertyModel> HEADER =
                new WritableObjectPropertyKey<>("header");
        static final WritableObjectPropertyKey<PropertyModel> IDP_SIGNIN =
                new WritableObjectPropertyKey<>("idp_signin");
        static final WritableObjectPropertyKey<PropertyModel> ERROR_TEXT =
                new WritableObjectPropertyKey<>("error_text");
        static final WritableObjectPropertyKey<PropertyModel> ADD_ACCOUNT_BUTTON =
                new WritableObjectPropertyKey<>("add_account_btn");
        static final WritableObjectPropertyKey<PropertyModel> ACCOUNT_CHIP =
                new WritableObjectPropertyKey<>("account_chip");
        static final WritableBooleanPropertyKey SPINNER_ENABLED =
                new WritableBooleanPropertyKey("spinner_enabled");

        static final PropertyKey[] ALL_KEYS = {
            CONTINUE_BUTTON,
            DATA_SHARING_CONSENT,
            HEADER,
            IDP_SIGNIN,
            ERROR_TEXT,
            ADD_ACCOUNT_BUTTON,
            ACCOUNT_CHIP,
            SPINNER_ENABLED
        };

        private ItemProperties() {}
    }

    private AccountSelectionProperties() {}
}

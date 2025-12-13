// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
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
    public static final int ITEM_TYPE_LOGIN = 2;
    public static final int ITEM_TYPE_SEPARATOR = 3;

    /**
     * The data needed for a button in the AccountSelection sheet. It may be a continue button for
     * an account, a login URL for an IDP, or an error dialog, so we need to include Account and IDP
     * information.
     */
    static class ButtonData {
        ButtonData(Account account, IdentityProviderMetadata idpMetadata) {
            mAccount = account;
            mIdpMetadata = idpMetadata;
        }

        public Account mAccount;
        public IdentityProviderMetadata mIdpMetadata;
    }
    ;

    /** Properties for an account entry in AccountSelection sheet. */
    static class AccountProperties {
        static class Avatar {
            // Display name is used to create a fallback monogram Icon.
            final String mDisplayName;
            final Bitmap mAvatar;
            final int mAvatarSize;

            Avatar(String displayName, @Nullable Bitmap avatar, int avatarSize) {
                mDisplayName = displayName;
                mAvatar = avatar;
                mAvatarSize = avatarSize;
            }
        }

        static final WritableObjectPropertyKey<Avatar> AVATAR =
                new WritableObjectPropertyKey<>("avatar");
        static final ReadableObjectPropertyKey<Account> ACCOUNT =
                new ReadableObjectPropertyKey<>("account");
        static final ReadableBooleanPropertyKey SHOW_IDP =
                new ReadableBooleanPropertyKey("show_idp");
        static final ReadableObjectPropertyKey<Callback<ButtonData>> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {AVATAR, ACCOUNT, SHOW_IDP, ON_CLICK_LISTENER};

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
            REQUEST_PERMISSION_MODAL
        }

        static final ReadableObjectPropertyKey<Runnable> CLOSE_ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("close_on_click_listener");
        static final ReadableObjectPropertyKey<String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");
        static final ReadableObjectPropertyKey<String> RP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("rp_for_display");
        static final ReadableObjectPropertyKey<String> IFRAME_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("iframe_for_display");
        static final ReadableObjectPropertyKey<Bitmap> HEADER_ICON =
                new ReadableObjectPropertyKey<>("header_icon");
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
        static final ReadableBooleanPropertyKey IS_MULTIPLE_IDPS =
                new ReadableBooleanPropertyKey("is_multiple_idps");

        static final PropertyKey[] ALL_KEYS = {
            CLOSE_ON_CLICK_LISTENER,
            IDP_FOR_DISPLAY,
            RP_FOR_DISPLAY,
            IFRAME_FOR_DISPLAY,
            HEADER_ICON,
            RP_BRAND_ICON,
            TYPE,
            RP_CONTEXT,
            RP_MODE,
            IS_MULTIPLE_ACCOUNT_CHOOSER,
            SET_FOCUS_VIEW_CALLBACK,
            IS_MULTIPLE_IDPS
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

        static final ReadableObjectPropertyKey<DataSharingConsentProperties.Properties> PROPERTIES =
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
            public Callback<ButtonData> mOnClickListener;
            public HeaderProperties.HeaderType mHeaderType;
            public Callback<View> mSetFocusViewCallback;
        }

        static final ReadableObjectPropertyKey<ContinueButtonProperties.Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private ContinueButtonProperties() {}
    }

    /**
     * Properties defined here reflect the state of a login button in the AccountSelection sheet.
     */
    static class LoginButtonProperties {
        static class Properties {
            public IdentityProviderData mIdentityProvider;
            public Callback<ButtonData> mOnClickListener;
            public @RpMode.EnumType int mRpMode;
            public boolean mShowIdp;
        }

        static final ReadableObjectPropertyKey<LoginButtonProperties.Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private LoginButtonProperties() {}
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

        static final ReadableObjectPropertyKey<ErrorProperties.Properties> PROPERTIES =
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
        static final WritableBooleanPropertyKey DRAGBAR_HANDLE_VISIBLE =
                new WritableBooleanPropertyKey("dragbar_handle_visible");

        static final PropertyKey[] ALL_KEYS = {
            CONTINUE_BUTTON,
            DATA_SHARING_CONSENT,
            HEADER,
            IDP_SIGNIN,
            ERROR_TEXT,
            ADD_ACCOUNT_BUTTON,
            ACCOUNT_CHIP,
            SPINNER_ENABLED,
            DRAGBAR_HANDLE_VISIBLE
        };

        private ItemProperties() {}
    }

    private AccountSelectionProperties() {}
}

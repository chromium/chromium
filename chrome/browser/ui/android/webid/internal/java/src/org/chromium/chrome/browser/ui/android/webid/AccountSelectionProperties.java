// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.function.Consumer;

/** Properties defined here reflect the state of the AccountSelection-components. */
@NullMarked
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
        ButtonData(@Nullable Account account, @Nullable IdentityProviderMetadata idpMetadata) {
            mAccount = account;
            mIdpMetadata = idpMetadata;
        }

        public @Nullable Account mAccount;
        public @Nullable IdentityProviderMetadata mIdpMetadata;
    }

    /** Properties for an account entry in AccountSelection sheet. */
    static class AccountProperties {
        static class Avatar {
            // Display name is used to create a fallback monogram Icon.
            final String mDisplayName;
            final @Nullable Bitmap mAvatar;
            final int mAvatarSize;

            Avatar(String displayName, @Nullable Bitmap avatar, int avatarSize) {
                mDisplayName = displayName;
                mAvatar = avatar;
                mAvatarSize = avatarSize;
            }
        }

        static final WritableObjectPropertyKey<@Nullable Avatar> AVATAR =
                new WritableObjectPropertyKey<>("avatar");
        static final ReadableObjectPropertyKey<Account> ACCOUNT =
                new ReadableObjectPropertyKey<>("account");
        static final ReadableBooleanPropertyKey SHOW_IDP =
                new ReadableBooleanPropertyKey("show_idp");
        static final ReadableObjectPropertyKey<@Nullable Callback<ButtonData>> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {AVATAR, ACCOUNT, SHOW_IDP, ON_CLICK_LISTENER};

        private AccountProperties() {}
    }

    /** Properties defined here reflect the state of the header in the AccountSelection sheet. */
    static class HeaderProperties {
        @IntDef({
            HeaderType.NONE,
            HeaderType.SIGN_IN,
            HeaderType.VERIFY,
            HeaderType.VERIFY_AUTO_REAUTHN,
            HeaderType.SIGN_IN_TO_IDP_STATIC,
            HeaderType.SIGN_IN_ERROR,
            HeaderType.LOADING,
            HeaderType.REQUEST_PERMISSION_MODAL
        })
        @Target(ElementType.TYPE_USE)
        @Retention(RetentionPolicy.SOURCE)
        @interface HeaderType {
            /** No sheet has been shown yet. */
            int NONE = 0;

            int SIGN_IN = 1;
            int VERIFY = 2;
            int VERIFY_AUTO_REAUTHN = 3;
            int SIGN_IN_TO_IDP_STATIC = 4;
            int SIGN_IN_ERROR = 5;
            int LOADING = 6;
            int REQUEST_PERMISSION_MODAL = 7;
        }

        static final ReadableObjectPropertyKey<@Nullable Runnable> CLOSE_ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("close_on_click_listener");
        static final ReadableObjectPropertyKey<@Nullable String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");
        static final ReadableObjectPropertyKey<String> RP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("rp_for_display");
        static final ReadableObjectPropertyKey<String> IFRAME_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("iframe_for_display");
        static final ReadableObjectPropertyKey<@Nullable Bitmap> HEADER_ICON =
                new ReadableObjectPropertyKey<>("header_icon");
        static final ReadableObjectPropertyKey<@Nullable Bitmap> RP_BRAND_ICON =
                new ReadableObjectPropertyKey<>("rp_brand_icon");
        static final ReadableIntPropertyKey TYPE = new ReadableIntPropertyKey("type");
        static final ReadableIntPropertyKey RP_CONTEXT = new ReadableIntPropertyKey("rp_context");
        static final ReadableObjectPropertyKey<Integer> RP_MODE =
                new ReadableObjectPropertyKey<>("rp_mode");
        static final ReadableBooleanPropertyKey IS_MULTIPLE_ACCOUNT_CHOOSER =
                new ReadableBooleanPropertyKey("is_multiple_account_chooser");
        static final ReadableObjectPropertyKey<@Nullable Callback<View>> SET_FOCUS_VIEW_CALLBACK =
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
            public final @Nullable String mIdpForDisplay;
            public final GURL mTermsOfServiceUrl;
            public final GURL mPrivacyPolicyUrl;
            public final Consumer<Context> mTermsOfServiceClickCallback;
            public final Consumer<Context> mPrivacyPolicyClickCallback;
            public final @Nullable Callback<View> mSetFocusViewCallback;
            public final @IdentityRequestDialogDisclosureField int[] mDisclosureFields;

            Properties(
                    @Nullable String idpForDisplay,
                    GURL termsOfServiceUrl,
                    GURL privacyPolicyUrl,
                    Consumer<Context> termsOfServiceClickCallback,
                    Consumer<Context> privacyPolicyClickCallback,
                    @Nullable Callback<View> setFocusViewCallback,
                    @IdentityRequestDialogDisclosureField int[] disclosureFields) {
                mIdpForDisplay = idpForDisplay;
                mTermsOfServiceUrl = termsOfServiceUrl;
                mPrivacyPolicyUrl = privacyPolicyUrl;
                mTermsOfServiceClickCallback = termsOfServiceClickCallback;
                mPrivacyPolicyClickCallback = privacyPolicyClickCallback;
                mSetFocusViewCallback = setFocusViewCallback;
                mDisclosureFields = disclosureFields;
            }
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
            public final @Nullable Account mAccount;
            public final IdentityProviderMetadata mIdpMetadata;
            public final Callback<ButtonData> mOnClickListener;
            public final @HeaderProperties.HeaderType int mHeaderType;
            public final @Nullable Callback<View> mSetFocusViewCallback;

            Properties(
                    @Nullable Account account,
                    IdentityProviderMetadata idpMetadata,
                    Callback<ButtonData> onClickListener,
                    @HeaderProperties.HeaderType int headerType,
                    @Nullable Callback<View> setFocusViewCallback) {
                mAccount = account;
                mIdpMetadata = idpMetadata;
                mOnClickListener = onClickListener;
                mHeaderType = headerType;
                mSetFocusViewCallback = setFocusViewCallback;
            }
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
            public final IdentityProviderData mIdentityProvider;
            public final Callback<ButtonData> mOnClickListener;
            public final @RpMode.EnumType int mRpMode;
            public final boolean mShowIdp;

            Properties(
                    IdentityProviderData identityProvider,
                    Callback<ButtonData> onClickListener,
                    @RpMode.EnumType int rpMode,
                    boolean showIdp) {
                mIdentityProvider = identityProvider;
                mOnClickListener = onClickListener;
                mRpMode = rpMode;
                mShowIdp = showIdp;
            }
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
        static final ReadableObjectPropertyKey<@Nullable String> IDP_FOR_DISPLAY =
                new ReadableObjectPropertyKey<>("idp_for_display");

        static final PropertyKey[] ALL_KEYS = {IDP_FOR_DISPLAY};

        private IdpSignInProperties() {}
    }

    /**
     * Properties defined here reflect the state of the error text in the AccountSelection sheet.
     */
    static class ErrorProperties {
        static class Properties {
            public final @Nullable String mIdpForDisplay;
            public final String mRpForDisplay;
            public final IdentityCredentialTokenError mError;
            public final @Nullable Runnable mMoreDetailsClickRunnable;

            Properties(
                    @Nullable String idpForDisplay,
                    String rpForDisplay,
                    IdentityCredentialTokenError error,
                    @Nullable Runnable moreDetailsClickRunnable) {
                mIdpForDisplay = idpForDisplay;
                mRpForDisplay = rpForDisplay;
                mError = error;
                mMoreDetailsClickRunnable = moreDetailsClickRunnable;
            }
        }

        static final ReadableObjectPropertyKey<ErrorProperties.Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private ErrorProperties() {}
    }

    /** Properties defined here reflect sections in the FedCM bottom sheet. */
    static class ItemProperties {
        static final WritableObjectPropertyKey<@Nullable PropertyModel> CONTINUE_BUTTON =
                new WritableObjectPropertyKey<>("continue_btn");
        static final WritableObjectPropertyKey<@Nullable PropertyModel> DATA_SHARING_CONSENT =
                new WritableObjectPropertyKey<>("data_sharing_consent");
        static final WritableObjectPropertyKey<PropertyModel> HEADER =
                new WritableObjectPropertyKey<>("header");
        static final WritableObjectPropertyKey<@Nullable PropertyModel> IDP_SIGNIN =
                new WritableObjectPropertyKey<>("idp_signin");
        static final WritableObjectPropertyKey<@Nullable PropertyModel> ERROR_TEXT =
                new WritableObjectPropertyKey<>("error_text");
        static final WritableObjectPropertyKey<@Nullable PropertyModel> ADD_ACCOUNT_BUTTON =
                new WritableObjectPropertyKey<>("add_account_btn");
        static final WritableObjectPropertyKey<@Nullable PropertyModel> ACCOUNT_CHIP =
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

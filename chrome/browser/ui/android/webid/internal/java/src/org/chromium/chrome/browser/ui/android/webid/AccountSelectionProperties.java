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
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** Properties defined here reflect the state of the AccountSelection-components. */
@NullMarked
class AccountSelectionProperties {
    public static final int ITEM_TYPE_ACCOUNT = 1;
    public static final int ITEM_TYPE_LOGIN = 2;
    public static final int ITEM_TYPE_SEPARATOR = 3;
    public static final int ITEM_TYPE_IDP = 4;
    public static final int ITEM_TYPE_SHOW_IDP_PICKER = 5;

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
            HeaderType.REQUEST_PERMISSION_MODAL,
            HeaderType.IDP_PICKER
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
            int IDP_PICKER = 8;
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
        // Whether the current state of the dialog involves multiple IDPs, as
        // AccountSelectionMediator#mIsShowingMultipleIdps. False once narrowed to a single
        // account's IDP.
        static final ReadableBooleanPropertyKey IS_SHOWING_MULTIPLE_IDPS =
                new ReadableBooleanPropertyKey("is_showing_multiple_idps");
        // Whether this is a multi-IDP request. Unlike IS_SHOWING_MULTIPLE_IDPS, it stays set
        // after the dialog narrows to a single account's IDP.
        static final ReadableBooleanPropertyKey IS_MULTI_IDP_REQUEST =
                new ReadableBooleanPropertyKey("is_multi_idp_request");

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
            IS_SHOWING_MULTIPLE_IDPS,
            IS_MULTI_IDP_REQUEST
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
            // The identity provider's account management page, privacy policy and terms of
            // service, shown as links below the disclosure text in the active mode container UI.
            // Each is empty if it isn't shown.
            public final GURL mIdpAccountManagementUrl;
            public final GURL mIdpPrivacyPolicyUrl;
            public final GURL mIdpTermsOfServiceUrl;
            // Called with the URL of the identity provider's link that the user clicks.
            public final BiConsumer<Context, GURL> mIdpLinkClickCallback;
            public final @Nullable Callback<View> mSetFocusViewCallback;
            public final @IdentityRequestDialogDisclosureField int[] mDisclosureFields;

            Properties(
                    @Nullable String idpForDisplay,
                    GURL termsOfServiceUrl,
                    GURL privacyPolicyUrl,
                    Consumer<Context> termsOfServiceClickCallback,
                    Consumer<Context> privacyPolicyClickCallback,
                    GURL idpAccountManagementUrl,
                    GURL idpPrivacyPolicyUrl,
                    GURL idpTermsOfServiceUrl,
                    BiConsumer<Context, GURL> idpLinkClickCallback,
                    @Nullable Callback<View> setFocusViewCallback,
                    @IdentityRequestDialogDisclosureField int[] disclosureFields) {
                mIdpForDisplay = idpForDisplay;
                mTermsOfServiceUrl = termsOfServiceUrl;
                mPrivacyPolicyUrl = privacyPolicyUrl;
                mTermsOfServiceClickCallback = termsOfServiceClickCallback;
                mPrivacyPolicyClickCallback = privacyPolicyClickCallback;
                mIdpAccountManagementUrl = idpAccountManagementUrl;
                mIdpPrivacyPolicyUrl = idpPrivacyPolicyUrl;
                mIdpTermsOfServiceUrl = idpTermsOfServiceUrl;
                mIdpLinkClickCallback = idpLinkClickCallback;
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

    /** Properties for an identity provider entry in the active-mode IDP picker. */
    static class IdpProperties {
        static class Properties {
            public final IdentityProviderData mIdentityProvider;
            public final Callback<ButtonData> mOnClickListener;

            Properties(
                    IdentityProviderData identityProvider, Callback<ButtonData> onClickListener) {
                mIdentityProvider = identityProvider;
                mOnClickListener = onClickListener;
            }
        }

        static final ReadableObjectPropertyKey<IdpProperties.Properties> PROPERTIES =
                new ReadableObjectPropertyKey<>("properties");

        static final PropertyKey[] ALL_KEYS = {PROPERTIES};

        private IdpProperties() {}
    }

    /** Properties for the "Use a different account" row that opens the active-mode IDP picker. */
    static class ShowIdpPickerProperties {
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        static final PropertyKey[] ALL_KEYS = {ON_CLICK_LISTENER};

        private ShowIdpPickerProperties() {}
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

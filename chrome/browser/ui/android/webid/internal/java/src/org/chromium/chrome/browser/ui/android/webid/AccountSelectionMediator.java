// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.os.Handler;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AutoSignInCancelButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mVisible;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final ModelList mSheetItems;
    private final ImageFetcher mImageFetcher;
    private final LargeIconBridge mLargeIconBridge;
    private final @Px int mDesiredAvatarSize;
    private final @Px int mDesiredIconSize;

    private final BottomSheetController mBottomSheetController;
    private final AccountSelectionBottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Handler mAutoSignInTaskHandler = new Handler();
    // TODO(yigu): Increase the time after adding a continue button for users to
    // proceed. Eventually this should be specified by IDPs.
    private static final int AUTO_SIGN_IN_CANCELLATION_TIMER_MS = 5000;

    private GURL mRpUrl;
    private GURL mIdpUrl;
    private IdentityProviderMetadata mIdpMetadata;
    private ClientIdMetadata mClientMetadata;

    // All of the user's accounts.
    private List<Account> mAccounts;

    // The account that the user has selected.
    private Account mSelectedAccount;

    AccountSelectionMediator(AccountSelectionComponent.Delegate delegate, ModelList sheetItems,
            BottomSheetController bottomSheetController,
            AccountSelectionBottomSheetContent bottomSheetContent, ImageFetcher imageFetcher,
            @Px int desiredAvatarSize, LargeIconBridge largeIconBridge, @Px int desiredIconSize) {
        assert delegate != null;
        mVisible = false;
        mDelegate = delegate;
        mSheetItems = sheetItems;
        mImageFetcher = imageFetcher;
        mDesiredAvatarSize = desiredAvatarSize;
        mLargeIconBridge = largeIconBridge;
        mDesiredIconSize = desiredIconSize;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;

        mBottomSheetContent.setBackPressHandler(() -> { return handleBackPress(); });

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            // TODO(majidvp): We should override #onSheetStateChanged() and react to HIDDEN state
            // since closed is a legacy fixture that can get out of sync with the state is some
            // situations. https://crbug.com/1215174
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                mBottomSheetController.removeObserver(mBottomSheetObserver);

                if (!mVisible) return;
                onDismissed(reason);
            }
        };
    }

    private boolean handleBackPress() {
        if (mVisible && mSelectedAccount != null && mAccounts.size() != 1) {
            showAccountsInternal(mRpUrl, mIdpUrl, mAccounts, /*selectedAccount=*/null, mIdpMetadata,
                    mClientMetadata, /*isAutoSignIn=*/false);
            return true;
        }
        return false;
    }

    // This method should not be used when the VERIFY header is needed.
    private void addHeader(GURL rpUrl, GURL idpUrl, List<Account> accounts) {
        boolean useSignInHeader = false;
        for (Account account : accounts) {
            if (!account.isSignIn()) continue;
            useSignInHeader = true;
            break;
        }
        HeaderType headerType;
        if (useSignInHeader) {
            headerType = HeaderType.SIGN_IN;
        } else {
            headerType =
                    accounts.size() == 1 ? HeaderType.SINGLE_ACCOUNT : HeaderType.MULTIPLE_ACCOUNT;
        }
        String formattedRpUrl =
                UrlFormatter.formatUrlForSecurityDisplay(rpUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        String formattedIdpUrl =
                UrlFormatter.formatUrlForSecurityDisplay(idpUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS);

        Runnable closeOnClickRunnable = () -> {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        };

        // We remove the HTTPS from URL since it is the only protocol that is
        // allowed with WebID.
        mSheetItems.add(new ListItem(ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                        .with(HeaderProperties.FORMATTED_IDP_URL, formattedIdpUrl)
                        .with(HeaderProperties.FORMATTED_RP_URL, formattedRpUrl)
                        .with(HeaderProperties.TYPE, headerType)
                        .build()));
    }

    private void addAccounts(GURL idpUrl, List<Account> accounts, boolean areAccountsClickable) {
        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account, areAccountsClickable);
            mSheetItems.add(new ListItem(ItemType.ACCOUNT, model));
            requestIconOrFallbackImage(model, idpUrl);
            requestAvatarImage(model);
        }
    }

    private void addAutoSignInCancelButton(Account account) {
        final PropertyModel cancelBtnModel = createAutoSignInCancelBtnItem();
        mSheetItems.add(new ListItem(ItemType.AUTO_SIGN_IN_CANCEL_BUTTON, cancelBtnModel));
    }

    private void addContinueButton(Account account, GURL rpUrl, GURL idpUrl,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata) {
        // Shows the continue button for both sign-up and non auto-sign-in.
        final PropertyModel continueBtnModel = createContinueBtnItem(account, idpMetadata);
        mSheetItems.add(new ListItem(ItemType.CONTINUE_BUTTON, continueBtnModel));

        // Only show the user data sharing consent text for sign up.
        if (!account.isSignIn()) {
            mSheetItems.add(new ListItem(ItemType.DATA_SHARING_CONSENT,
                    createDataSharingConsentItem(rpUrl, idpUrl, clientMetadata)));
        }
    }

    void showVerifySheet(Account account) {
        mSheetItems.clear();

        Runnable closeOnClickRunnable = () -> {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        };
        mSheetItems.add(new ListItem(ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                        .with(HeaderProperties.TYPE, HeaderType.VERIFY)
                        .build()));

        addAccounts(mIdpUrl, Arrays.asList(account), /*areAccountsClickable=*/false);
        showContent();
    }

    void hideBottomSheet() {
        if (mVisible) hideContent();
    }

    void showAccounts(GURL rpUrl, GURL idpUrl, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoSignIn) {
        Account selectedAccount = accounts.size() == 1 ? accounts.get(0) : null;
        showAccountsInternal(rpUrl, idpUrl, accounts, selectedAccount, idpMetadata, clientMetadata,
                isAutoSignIn);
    }

    private void showAccountsInternal(GURL rpUrl, GURL idpUrl, List<Account> accounts,
            Account selectedAccount, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata, boolean isAutoSignIn) {
        mSheetItems.clear();

        mRpUrl = rpUrl;
        mIdpUrl = idpUrl;
        mAccounts = accounts;
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;
        mSelectedAccount = selectedAccount;

        if (mSelectedAccount != null) {
            accounts = Arrays.asList(selectedAccount);
        }

        addHeader(rpUrl, idpUrl, accounts);
        addAccounts(idpUrl, accounts, /*areAccountsClickable=*/mSelectedAccount == null);

        if (isAutoSignIn) {
            assert mSelectedAccount != null;
            assert mSelectedAccount.isSignIn();
            addAutoSignInCancelButton(mSelectedAccount);
            mAutoSignInTaskHandler.postDelayed(
                    () -> onAccountSelected(mSelectedAccount), AUTO_SIGN_IN_CANCELLATION_TIMER_MS);
        } else if (mSelectedAccount != null) {
            addContinueButton(mSelectedAccount, rpUrl, idpUrl, idpMetadata, clientMetadata);
        }

        showContent();
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    @VisibleForTesting
    void showContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            if (mVisible) return;

            mVisible = true;
            mBottomSheetController.addObserver(mBottomSheetObserver);
        } else {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        }
    }

    /**
     * Requests to hide the bottom sheet.
     */
    void hideContent() {
        mVisible = false;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    private void requestAvatarImage(PropertyModel accountModel) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final String name = account.getName();
        final String avatarURL = account.getPictureUrl().getSpec();

        if (!avatarURL.isEmpty()) {
            ImageFetcher.Params params = ImageFetcher.Params.create(avatarURL,
                    ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME, mDesiredAvatarSize,
                    mDesiredAvatarSize);

            mImageFetcher.fetchImage(params, bitmap -> {
                accountModel.set(AccountProperties.AVATAR,
                        new AccountProperties.Avatar(name, bitmap, mDesiredAvatarSize));
            });
        } else {
            accountModel.set(AccountProperties.AVATAR,
                    new AccountProperties.Avatar(name, null, mDesiredAvatarSize));
        }
    }

    private void requestIconOrFallbackImage(PropertyModel accountModel, GURL idpUrl) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final LargeIconCallback setIcon = (icon, fallbackColor, hasDefaultColor, type) -> {
            accountModel.set(AccountProperties.FAVICON_OR_FALLBACK,
                    new AccountProperties.FaviconOrFallback(
                            idpUrl, icon, fallbackColor, mDesiredIconSize));
        };
        mLargeIconBridge.getLargeIconForUrl(idpUrl, mDesiredIconSize, setIcon);
    }

    boolean isVisible() {
        return mVisible;
    }

    void onAccountSelected(Account selectedAccount) {
        if (!mVisible) return;

        if (mSelectedAccount == null && !selectedAccount.isSignIn()) {
            showAccountsInternal(mRpUrl, mIdpUrl, mAccounts, selectedAccount, mIdpMetadata,
                    mClientMetadata, /*isAutoSignIn=*/false);
            return;
        }

        mDelegate.onAccountSelected(selectedAccount);
        showVerifySheet(selectedAccount);
    }

    void onDismissed(@StateChangeReason int reason) {
        hideContent();
        mDelegate.onDismissed();
    }

    void onAutoSignInCancelled() {
        hideContent();
        mDelegate.onAutoSignInCancelled();
    }

    private PropertyModel createAccountItem(Account account, boolean isAccountClickable) {
        PropertyModel.Builder modelBuilder = new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                                     .with(AccountProperties.ACCOUNT, account);
        if (isAccountClickable) {
            modelBuilder.with(AccountProperties.ON_CLICK_LISTENER, this::onAccountSelected);
        }
        return modelBuilder.build();
    }

    private PropertyModel createContinueBtnItem(
            Account account, IdentityProviderMetadata idpMetadata) {
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.IDP_METADATA, idpMetadata)
                .with(ContinueButtonProperties.ACCOUNT, account)
                .with(ContinueButtonProperties.ON_CLICK_LISTENER, this::onAccountSelected)
                .build();
    }

    private PropertyModel createAutoSignInCancelBtnItem() {
        return new PropertyModel.Builder(AutoSignInCancelButtonProperties.ALL_KEYS)
                .with(AutoSignInCancelButtonProperties.ON_CLICK_LISTENER,
                        this::onAutoSignInCancelled)
                .build();
    }

    private PropertyModel createDataSharingConsentItem(
            GURL rpUrl, GURL idpUrl, ClientIdMetadata metadata) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mFormattedIdpUrl =
                UrlFormatter.formatUrlForSecurityDisplay(idpUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        properties.mFormattedRpUrl =
                UrlFormatter.formatUrlForSecurityDisplay(rpUrl, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        properties.mTermsOfServiceUrl = metadata.getTermsOfServiceUrl().getValidSpecOrEmpty();
        properties.mPrivacyPolicyUrl = metadata.getPrivacyPolicyUrl().getValidSpecOrEmpty();

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }
}

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
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mVisible;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final PropertyModel mModel;
    private final ModelList mSheetAccountItems;
    private final ImageFetcher mImageFetcher;
    private final @Px int mDesiredAvatarSize;

    private final BottomSheetController mBottomSheetController;
    private final AccountSelectionBottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Handler mAutoSignInTaskHandler = new Handler();
    // TODO(yigu): Increase the time after adding a continue button for users to
    // proceed. Eventually this should be specified by IDPs.
    private static final int AUTO_SIGN_IN_CANCELLATION_TIMER_MS = 5000;

    private String mRpEtldPlusOne;
    private String mIdpEtldPlusOne;
    private IdentityProviderMetadata mIdpMetadata;
    private ClientIdMetadata mClientMetadata;

    // All of the user's accounts.
    private List<Account> mAccounts;

    // The account that the user has selected.
    private Account mSelectedAccount;

    AccountSelectionMediator(AccountSelectionComponent.Delegate delegate, PropertyModel model,
            ModelList sheetAccountItems, BottomSheetController bottomSheetController,
            AccountSelectionBottomSheetContent bottomSheetContent, ImageFetcher imageFetcher,
            @Px int desiredAvatarSize) {
        assert delegate != null;
        mVisible = false;
        mDelegate = delegate;
        mModel = model;
        mSheetAccountItems = sheetAccountItems;
        mImageFetcher = imageFetcher;
        mDesiredAvatarSize = desiredAvatarSize;
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
            mSelectedAccount = null;
            showAccountsInternal(mRpEtldPlusOne, mIdpEtldPlusOne, mAccounts, mIdpMetadata,
                    mClientMetadata, /*isAutoSignIn=*/false, /*focusItem=*/ItemProperties.HEADER);
            return true;
        }
        return false;
    }

    private PropertyModel createHeaderItem(HeaderType headerType, String rpEtldPlusOne,
            String idpEtldPlusOne, IdentityProviderMetadata idpMetadata) {
        String formattedRpEtldPlusOne = UrlFormatter.formatUrlForSecurityDisplay(
                UrlFormatter.fixupUrl(rpEtldPlusOne), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        String formattedIdpEtldPlusOne = UrlFormatter.formatUrlForSecurityDisplay(
                UrlFormatter.fixupUrl(idpEtldPlusOne), SchemeDisplay.OMIT_HTTP_AND_HTTPS);

        Runnable closeOnClickRunnable = () -> {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        };

        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.IDP_BRAND_ICON, idpMetadata.getBrandIcon())
                .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                .with(HeaderProperties.FORMATTED_IDP_ETLD_PLUS_ONE, formattedIdpEtldPlusOne)
                .with(HeaderProperties.FORMATTED_RP_ETLD_PLUS_ONE, formattedRpEtldPlusOne)
                .with(HeaderProperties.TYPE, headerType)
                .build();
    }

    private void updateAccounts(
            String idpEtldPlusOne, List<Account> accounts, boolean areAccountsClickable) {
        mSheetAccountItems.clear();

        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account, areAccountsClickable);
            mSheetAccountItems.add(
                    new ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT, model));
            requestAvatarImage(model);
        }
    }

    void showVerifySheet(Account account) {
        updateSheet(HeaderType.VERIFY, Arrays.asList(account), /*areAccountsClickable=*/false,
                /* focusItem=*/ItemProperties.HEADER);
    }

    void hideBottomSheet() {
        if (mVisible) hideContent();
    }

    void showAccounts(String rpEtldPlusOne, String idpEtldPlusOne, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoSignIn) {
        mSelectedAccount = accounts.size() == 1 ? accounts.get(0) : null;
        showAccountsInternal(rpEtldPlusOne, idpEtldPlusOne, accounts, idpMetadata, clientMetadata,
                isAutoSignIn, /*focusItem=*/ItemProperties.HEADER);
    }

    private void showAccountsInternal(String rpEtldPlusOne, String idpEtldPlusOne,
            List<Account> accounts, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata, boolean isAutoSignIn, PropertyKey focusItem) {
        mRpEtldPlusOne = rpEtldPlusOne;
        mIdpEtldPlusOne = idpEtldPlusOne;
        mAccounts = accounts;
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;

        if (mSelectedAccount != null) {
            accounts = Arrays.asList(mSelectedAccount);
        }

        HeaderType headerType = isAutoSignIn ? HeaderType.AUTO_SIGN_IN : HeaderType.SIGN_IN;
        updateSheet(
                headerType, accounts, /*areAccountsClickable=*/mSelectedAccount == null, focusItem);
    }

    private void updateSheet(HeaderType headerType, List<Account> accounts,
            boolean areAccountsClickable, PropertyKey focusItem) {
        updateAccounts(mIdpEtldPlusOne, accounts, areAccountsClickable);

        PropertyModel headerModel =
                createHeaderItem(headerType, mRpEtldPlusOne, mIdpEtldPlusOne, mIdpMetadata);
        mModel.set(ItemProperties.HEADER, headerModel);

        boolean isContinueButtonVisible = false;
        boolean isDataSharingConsentVisible = false;
        if (headerType == HeaderType.SIGN_IN && mSelectedAccount != null) {
            isContinueButtonVisible = true;
            // Only show the user data sharing consent text for sign up.
            isDataSharingConsentVisible = !mSelectedAccount.isSignIn();
        }

        if (headerType == HeaderType.AUTO_SIGN_IN) {
            assert mSelectedAccount != null;
            assert mSelectedAccount.isSignIn();

            mModel.set(ItemProperties.AUTO_SIGN_IN_CANCEL_BUTTON, createAutoSignInCancelBtnItem());
            mAutoSignInTaskHandler.postDelayed(
                    () -> onAccountSelected(mSelectedAccount), AUTO_SIGN_IN_CANCELLATION_TIMER_MS);
        } else {
            mModel.set(ItemProperties.AUTO_SIGN_IN_CANCEL_BUTTON, null);
        }

        mModel.set(ItemProperties.CONTINUE_BUTTON,
                isContinueButtonVisible ? createContinueBtnItem(mSelectedAccount, mIdpMetadata)
                                        : null);
        mModel.set(ItemProperties.DATA_SHARING_CONSENT,
                isDataSharingConsentVisible
                        ? createDataSharingConsentItem(mIdpEtldPlusOne, mClientMetadata)
                        : null);

        showContent();
        mBottomSheetContent.focusForAccessibility(focusItem);
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

    boolean isVisible() {
        return mVisible;
    }

    void onAccountSelected(Account selectedAccount) {
        if (!mVisible) return;

        Account oldSelectedAccount = mSelectedAccount;
        mSelectedAccount = selectedAccount;
        if (oldSelectedAccount == null && !mSelectedAccount.isSignIn()) {
            showAccountsInternal(mRpEtldPlusOne, mIdpEtldPlusOne, mAccounts, mIdpMetadata,
                    mClientMetadata, /*isAutoSignIn=*/false,
                    /*focusItem=*/ItemProperties.CONTINUE_BUTTON);
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
        return new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                .with(AccountProperties.ACCOUNT, account)
                .with(AccountProperties.ON_CLICK_LISTENER,
                        isAccountClickable ? this::onAccountSelected : null)
                .build();
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
            String idpEtldPlusOne, ClientIdMetadata metadata) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mFormattedIdpEtldPlusOne = UrlFormatter.formatUrlForSecurityDisplay(
                UrlFormatter.fixupUrl(idpEtldPlusOne), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        properties.mTermsOfServiceUrl = metadata.getTermsOfServiceUrl().getValidSpecOrEmpty();
        properties.mPrivacyPolicyUrl = metadata.getPrivacyPolicyUrl().getValidSpecOrEmpty();

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }
}

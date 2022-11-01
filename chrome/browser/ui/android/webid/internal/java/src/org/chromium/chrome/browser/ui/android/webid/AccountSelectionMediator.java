// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Px;

import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mRegisteredObservers;
    private boolean mWasDismissed;
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

    private HeaderType mHeaderType;
    private String mRpForDisplay;
    private String mIdpForDisplay;
    private IdentityProviderMetadata mIdpMetadata;
    private Bitmap mBrandIcon;
    private ClientIdMetadata mClientMetadata;

    // All of the user's accounts.
    private List<Account> mAccounts;

    // The account that the user has selected.
    private Account mSelectedAccount;

    private KeyboardVisibilityListener mKeyboardVisibilityListener =
            new KeyboardVisibilityListener() {
                @Override
                public void keyboardVisibilityChanged(boolean isShowing) {
                    if (isShowing) {
                        onDismissed(IdentityRequestDialogDismissReason.VIRTUAL_KEYBOARD_SHOWN);
                    }
                }
            };

    AccountSelectionMediator(AccountSelectionComponent.Delegate delegate, PropertyModel model,
            ModelList sheetAccountItems, BottomSheetController bottomSheetController,
            AccountSelectionBottomSheetContent bottomSheetContent, ImageFetcher imageFetcher,
            @Px int desiredAvatarSize) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mSheetAccountItems = sheetAccountItems;
        mImageFetcher = imageFetcher;
        mDesiredAvatarSize = desiredAvatarSize;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            // TODO(majidvp): We should override #onSheetStateChanged() and react to HIDDEN state
            // since closed is a legacy fixture that can get out of sync with the state is some
            // situations. https://crbug.com/1215174
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                mBottomSheetController.removeObserver(mBottomSheetObserver);

                if (mWasDismissed) return;

                @IdentityRequestDialogDismissReason
                int dismissReason = (reason == BottomSheetController.StateChangeReason.SWIPE)
                        ? IdentityRequestDialogDismissReason.SWIPE
                        : IdentityRequestDialogDismissReason.OTHER;
                onDismissed(dismissReason);
            }
        };
    }

    private void updateBackPressBehavior() {
        mBottomSheetContent.setCustomBackPressBehavior(
                !mWasDismissed && mSelectedAccount != null && mAccounts.size() != 1
                        ? this::handleBackPress
                        : null);
    }

    private void handleBackPress() {
        mSelectedAccount = null;
        showAccountsInternal(mRpForDisplay, mIdpForDisplay, mAccounts, mIdpMetadata,
                mClientMetadata, /*isAutoSignIn=*/false, /*focusItem=*/ItemProperties.HEADER);
    }

    private PropertyModel createHeaderItem(HeaderType headerType, String rpForDisplay,
            String idpForDisplay, IdentityProviderMetadata idpMetadata) {
        Runnable closeOnClickRunnable = () -> {
            onDismissed(IdentityRequestDialogDismissReason.CLOSE_BUTTON);

            RecordHistogram.recordBooleanHistogram(
                    "Blink.FedCm.CloseVerifySheet.Android", mHeaderType == HeaderType.VERIFY);
        };

        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.IDP_BRAND_ICON, mBrandIcon)
                .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                .with(HeaderProperties.IDP_FOR_DISPLAY, idpForDisplay)
                .with(HeaderProperties.RP_FOR_DISPLAY, rpForDisplay)
                .with(HeaderProperties.TYPE, headerType)
                .build();
    }

    private void updateAccounts(
            String idpForDisplay, List<Account> accounts, boolean areAccountsClickable) {
        mSheetAccountItems.clear();

        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account, areAccountsClickable);
            mSheetAccountItems.add(
                    new ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT, model));
            requestAvatarImage(model);
        }
    }

    void showVerifySheet(Account account) {
        mHeaderType = HeaderType.VERIFY;
        updateSheet(Arrays.asList(account), /*areAccountsClickable=*/false,
                /* focusItem=*/ItemProperties.HEADER);
    }

    void close() {
        if (!mWasDismissed) hideContent();
    }

    void showAccounts(String rpForDisplay, String idpForDisplay, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoSignIn) {
        if (!TextUtils.isEmpty(idpMetadata.getBrandIconUrl())) {
            // Use placeholder icon so that the header text wrapping does not change when the icon
            // is fetched.
            mBrandIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
            Canvas brandIconCanvas = new Canvas(mBrandIcon);
            brandIconCanvas.drawColor(Color.TRANSPARENT);
        }

        mSelectedAccount = accounts.size() == 1 ? accounts.get(0) : null;
        showAccountsInternal(rpForDisplay, idpForDisplay, accounts, idpMetadata, clientMetadata,
                isAutoSignIn, /*focusItem=*/ItemProperties.HEADER);

        if (!TextUtils.isEmpty(idpMetadata.getBrandIconUrl())) {
            int brandIconIdealSize = AccountSelectionBridge.getBrandIconIdealSize();
            ImageFetcher.Params params =
                    ImageFetcher.Params.createNoResizing(new GURL(idpMetadata.getBrandIconUrl()),
                            ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME,
                            brandIconIdealSize, brandIconIdealSize);

            mImageFetcher.fetchImage(params, bitmap -> {
                if (bitmap != null && bitmap.getWidth() == bitmap.getHeight()
                        && bitmap.getWidth() >= AccountSelectionBridge.getBrandIconMinimumSize()) {
                    mBrandIcon = bitmap;
                    updateHeader();
                }
            });
        }
    }

    private void showAccountsInternal(String rpForDisplay, String idpForDisplay,
            List<Account> accounts, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata, boolean isAutoSignIn, PropertyKey focusItem) {
        mRpForDisplay = rpForDisplay;
        mIdpForDisplay = idpForDisplay;
        mAccounts = accounts;
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;

        if (mSelectedAccount != null) {
            accounts = Arrays.asList(mSelectedAccount);
        }

        mHeaderType = isAutoSignIn ? HeaderType.AUTO_SIGN_IN : HeaderType.SIGN_IN;
        updateSheet(accounts, /*areAccountsClickable=*/mSelectedAccount == null, focusItem);
        updateBackPressBehavior();
    }

    private void updateSheet(
            List<Account> accounts, boolean areAccountsClickable, PropertyKey focusItem) {
        updateAccounts(mIdpForDisplay, accounts, areAccountsClickable);
        updateHeader();

        boolean isContinueButtonVisible = false;
        boolean isDataSharingConsentVisible = false;
        if (mHeaderType == HeaderType.SIGN_IN && mSelectedAccount != null) {
            isContinueButtonVisible = true;
            // Only show the user data sharing consent text for sign up.
            isDataSharingConsentVisible = !mSelectedAccount.isSignIn();
        }

        if (mHeaderType == HeaderType.AUTO_SIGN_IN) {
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
                        ? createDataSharingConsentItem(mIdpForDisplay, mClientMetadata)
                        : null);

        mBottomSheetContent.computeAndUpdateAccountListHeight();
        showContent();
        mBottomSheetContent.focusForAccessibility(focusItem);
    }

    private void updateHeader() {
        PropertyModel headerModel =
                createHeaderItem(mHeaderType, mRpForDisplay, mIdpForDisplay, mIdpMetadata);
        mModel.set(ItemProperties.HEADER, headerModel);
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    private void showContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            if (mRegisteredObservers) return;

            mRegisteredObservers = true;
            mBottomSheetController.addObserver(mBottomSheetObserver);
            KeyboardVisibilityDelegate.getInstance().addKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
        } else {
            onDismissed(IdentityRequestDialogDismissReason.OTHER);
        }
    }

    /**
     * Requests to hide the bottom sheet.
     */
    void hideContent() {
        mWasDismissed = true;
        KeyboardVisibilityDelegate.getInstance().removeKeyboardVisibilityListener(
                mKeyboardVisibilityListener);
        mBottomSheetController.hideContent(mBottomSheetContent, true);
        updateBackPressBehavior();
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

    boolean wasDismissed() {
        return mWasDismissed;
    }

    void onAccountSelected(Account selectedAccount) {
        if (mWasDismissed) return;

        Account oldSelectedAccount = mSelectedAccount;
        mSelectedAccount = selectedAccount;
        if (oldSelectedAccount == null && !mSelectedAccount.isSignIn()) {
            showAccountsInternal(mRpForDisplay, mIdpForDisplay, mAccounts, mIdpMetadata,
                    mClientMetadata, /*isAutoSignIn=*/false,
                    /*focusItem=*/ItemProperties.CONTINUE_BUTTON);
            return;
        }

        mDelegate.onAccountSelected(mIdpMetadata.getConfigUrl(), selectedAccount);
        showVerifySheet(selectedAccount);
        updateBackPressBehavior();
    }

    void onDismissed(@IdentityRequestDialogDismissReason int dismissReason) {
        hideContent();
        mDelegate.onDismissed(dismissReason);
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
            String idpForDisplay, ClientIdMetadata metadata) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mTermsOfServiceUrl = metadata.getTermsOfServiceUrl();
        properties.mPrivacyPolicyUrl = metadata.getPrivacyPolicyUrl();
        properties.mTermsOfServiceClickRunnable = () -> {
            RecordHistogram.recordBooleanHistogram(
                    "Blink.FedCm.SignUp.TermsOfServiceClicked", true);
        };
        properties.mPrivacyPolicyClickRunnable = () -> {
            RecordHistogram.recordBooleanHistogram("Blink.FedCm.SignUp.PrivacyPolicyClicked", true);
        };

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    /**
     * The following integers are used for histograms. Do not remove or modify existing values,
     * but you may add new values at the end and increase NUM_ENTRIES. This enum should be kept in
     * sync with SheetType in chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h
     * as well as with FedCmSheetType in tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        SheetType.ACCOUNT_SELECTION,
        SheetType.VERIFYING,
        SheetType.AUTO_REAUTHN,
        SheetType.SIGN_IN_TO_IDP_STATIC,
        SheetType.SIGN_IN_ERROR,
        SheetType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface SheetType {
        int ACCOUNT_SELECTION = 0;
        int VERIFYING = 1;
        int AUTO_REAUTHN = 2;
        int SIGN_IN_TO_IDP_STATIC = 3;
        int SIGN_IN_ERROR = 4;

        int NUM_ENTRIES = 5;
    }

    private boolean mRegisteredObservers;
    private boolean mWasDismissed;
    // Keeps track of the last bottom sheet seen by the BottomSheetObserver. Used to know whether a
    // sheet state change affects the BottomSheet owned by this object or not.
    private BottomSheetContent mLastSheetSeen;
    private final Tab mTab;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final PropertyModel mModel;
    private final ModelList mSheetAccountItems;
    private final ImageFetcher mImageFetcher;
    private final @Px int mDesiredAvatarSize;

    private final BottomSheetController mBottomSheetController;
    private final AccountSelectionBottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final TabObserver mTabObserver;

    // Amount of time during which we ignore inputs. Note that this is timed from when we invoke the
    // methods to show the accounts, so it does include any time spent animating the sheet into
    // view.
    public static final long POTENTIALLY_UNINTENDED_INPUT_THRESHOLD = 500;

    private HeaderType mHeaderType;
    private String mTopFrameForDisplay;
    private String mIframeForDisplay;
    private String mIdpForDisplay;
    private IdentityProviderMetadata mIdpMetadata;
    private Bitmap mBrandIcon;
    private ClientIdMetadata mClientMetadata;
    private String mRpContext;
    private IdentityCredentialTokenError mError;

    // All of the user's accounts.
    private List<Account> mAccounts;

    // The account that the user has selected.
    private Account mSelectedAccount;

    // Stores the value of SystemClock.elapsedRealtime() at the time in which the accounts are shown
    // to the user.
    private long mComponentShowTime;

    // Whether there is an open modal dialog. When a modal dialog is opened, this
    // mediator should not display any accounts until such dialog is closed.
    private boolean mIsModalDialogOpen;

    private KeyboardVisibilityListener mKeyboardVisibilityListener =
            new KeyboardVisibilityListener() {
                @Override
                public void keyboardVisibilityChanged(boolean isShowing) {
                    if (isShowing) {
                        mBottomSheetController.hideContent(mBottomSheetContent, true);
                    } else if (mTab.isUserInteractable()) {
                        showContent();
                    }
                }
            };

    AccountSelectionMediator(
            Tab tab,
            AccountSelectionComponent.Delegate delegate,
            PropertyModel model,
            ModelList sheetAccountItems,
            BottomSheetController bottomSheetController,
            AccountSelectionBottomSheetContent bottomSheetContent,
            ImageFetcher imageFetcher,
            @Px int desiredAvatarSize) {
        assert tab != null;
        mTab = tab;
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mSheetAccountItems = sheetAccountItems;
        mImageFetcher = imageFetcher;
        mDesiredAvatarSize = desiredAvatarSize;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mLastSheetSeen = mBottomSheetContent;

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    // Sends focus events to the relevant views for accessibility.
                    // TODO(crbug.com/1429345): Add tests for TalkBack on FedCM.
                    private void focusForAccessibility() {
                        View contentView =
                                mBottomSheetController.getCurrentSheetContent().getContentView();
                        assert contentView != null;
                        View continueButton =
                                contentView.findViewById(R.id.account_selection_continue_btn);

                        // TODO(crbug.com/1430240): Update SheetType and focus views for
                        // accessibility according to SheetType instead of number of accounts.
                        boolean isSingleAccountChooser = mAccounts != null && mAccounts.size() == 1;
                        View focusView =
                                continueButton != null
                                                && continueButton.isShown()
                                                && !isSingleAccountChooser
                                        ? continueButton
                                        : contentView.findViewById(R.id.header);

                        if (focusView == null) return;

                        focusView.requestFocus();
                        focusView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                    }

                    @Override
                    public void onSheetStateChanged(@SheetState int state, int reason) {
                        if (mLastSheetSeen != mBottomSheetContent) return;
                        if (mWasDismissed) return;

                        if (state == SheetState.HIDDEN) {
                            // BottomSheetController.StateChangeReason.NONE happens for instance
                            // when the user opens the tab switcher or when the user leaves
                            // Chrome. We do not want to dismiss in those cases.
                            if (reason == BottomSheetController.StateChangeReason.NONE) {
                                mBottomSheetController.hideContent(mBottomSheetContent, true);
                            } else {
                                super.onSheetClosed(reason);
                                @IdentityRequestDialogDismissReason
                                int dismissReason =
                                        (reason == BottomSheetController.StateChangeReason.SWIPE)
                                                ? IdentityRequestDialogDismissReason.SWIPE
                                                : IdentityRequestDialogDismissReason.OTHER;
                                onDismissed(dismissReason);
                            }
                            return;
                        }

                        if (state != SheetState.FULL) return;

                        // The bottom sheet programmatically requests focuses for accessibility when
                        // its contents are changed. If we call focusForAccessibility prior to
                        // onSheetStateChanged, the bottom sheet announcement would override the
                        // title or continue button announcement. Hence, focusForAccessibility is
                        // called here after the bottom sheet's focus-taking actions.
                        focusForAccessibility();
                    }

                    @Override
                    public void onSheetContentChanged(BottomSheetContent bottomSheet) {
                        // Keep track of the latest sheet seen. Since this method is invoked before
                        // onSheetStateChanged() when the sheet is swiped out, we do not clear
                        // |mLastSheetSeen| if |bottomSheet| is null.
                        if (bottomSheet != null) {
                            mLastSheetSeen = bottomSheet;
                        }
                    }
                };

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        assert tab == mTab;
                        onDismissed(IdentityRequestDialogDismissReason.OTHER);
                    }

                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        assert tab == mTab;
                        // |isInteractable| is true when the tab is not hidden and its view is
                        // attached to the window. We use this method instead of onShown() and
                        // onHidden() because this one is correctly invoked when the user enters
                        // tab switcher (the current tab is no longer interactable in this case).
                        if (isInteractable) {
                            showContent();
                        } else {
                            mBottomSheetController.hideContent(mBottomSheetContent, false);
                        }
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
        showAccountsInternal(
                mTopFrameForDisplay,
                mIframeForDisplay,
                mIdpForDisplay,
                mAccounts,
                mIdpMetadata,
                mClientMetadata,
                /* isAutoReauthn= */ false,
                mRpContext);
    }

    private PropertyModel createHeaderItem(
            HeaderType headerType,
            String topFrameForDisplay,
            String iframeForDisplay,
            String idpForDisplay,
            String rpContext) {
        Runnable closeOnClickRunnable =
                () -> {
                    onDismissed(IdentityRequestDialogDismissReason.CLOSE_BUTTON);

                    RecordHistogram.recordBooleanHistogram(
                            "Blink.FedCm.CloseVerifySheet.Android",
                            mHeaderType == HeaderType.VERIFY);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Blink.FedCm.ClosedSheetType.Android",
                            getSheetType(),
                            SheetType.NUM_ENTRIES);
                };

        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.IDP_BRAND_ICON, mBrandIcon)
                .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                .with(HeaderProperties.IDP_FOR_DISPLAY, idpForDisplay)
                .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, topFrameForDisplay)
                .with(HeaderProperties.IFRAME_FOR_DISPLAY, iframeForDisplay)
                .with(HeaderProperties.TYPE, headerType)
                .with(HeaderProperties.RP_CONTEXT, rpContext)
                .build();
    }

    private int getSheetType() {
        switch (mHeaderType) {
            case SIGN_IN:
                return SheetType.ACCOUNT_SELECTION;
            case VERIFY:
                return SheetType.VERIFYING;
            case VERIFY_AUTO_REAUTHN:
                return SheetType.AUTO_REAUTHN;
            case SIGN_IN_TO_IDP_STATIC:
                return SheetType.SIGN_IN_TO_IDP_STATIC;
            case SIGN_IN_ERROR:
                return SheetType.SIGN_IN_ERROR;
        }
        assert false; // NOTREACHED
        return SheetType.ACCOUNT_SELECTION;
    }

    private void updateAccounts(
            String idpForDisplay, List<Account> accounts, boolean areAccountsClickable) {
        mSheetAccountItems.clear();
        if (accounts == null) return;

        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account, areAccountsClickable);
            mSheetAccountItems.add(
                    new ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT, model));
            requestAvatarImage(model);
        }
    }

    /* Returns whether an input event being processed should be ignored due to it occurring too
     * close in time to the time in which the dialog was shown.
     */
    private boolean shouldInputBeProcessed() {
        assert mComponentShowTime != 0;
        long currentTime = SystemClock.elapsedRealtime();
        return currentTime - mComponentShowTime > POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;
    }

    /* Used to show placeholder icon so that the header text wrapping does not change when the icon
     * is fetched.
     */
    private void showPlaceholderIcon(IdentityProviderMetadata idpMetadata) {
        if (!TextUtils.isEmpty(idpMetadata.getBrandIconUrl())) {
            mBrandIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
            Canvas brandIconCanvas = new Canvas(mBrandIcon);
            brandIconCanvas.drawColor(Color.TRANSPARENT);
        }
    }

    private void showBrandIcon(IdentityProviderMetadata idpMetadata) {
        if (!TextUtils.isEmpty(idpMetadata.getBrandIconUrl())) {
            int brandIconIdealSize = AccountSelectionBridge.getBrandIconIdealSize();
            ImageFetcher.Params params =
                    ImageFetcher.Params.createNoResizing(
                            new GURL(idpMetadata.getBrandIconUrl()),
                            ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME,
                            brandIconIdealSize,
                            brandIconIdealSize);

            mImageFetcher.fetchImage(
                    params,
                    bitmap -> {
                        if (bitmap != null
                                && bitmap.getWidth() == bitmap.getHeight()
                                && bitmap.getWidth()
                                        >= AccountSelectionBridge.getBrandIconMinimumSize()) {
                            mBrandIcon = bitmap;
                            updateHeader();
                        }
                    });
        }
    }

    void showVerifySheet(Account account) {
        if (mHeaderType == HeaderType.SIGN_IN) {
            mHeaderType = HeaderType.VERIFY;
            updateSheet(Arrays.asList(account), /* areAccountsClickable= */ false);
        } else {
            // We call showVerifySheet() from updateSheet()->onAccountSelected() in this case, so do
            // not invoked updateSheet() as that would cause a loop and isn't needed.
            assert mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN;
        }
    }

    // Dismisses content without notifying the delegate. Should only be invoked during destruction.
    void close() {
        if (!mWasDismissed) dismissContent();
    }

    void showAccounts(
            String topFrameForDisplay,
            String iframeForDisplay,
            String idpForDisplay,
            List<Account> accounts,
            IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata,
            boolean isAutoReauthn,
            String rpContext) {
        showPlaceholderIcon(idpMetadata);
        mSelectedAccount = accounts.size() == 1 ? accounts.get(0) : null;
        showAccountsInternal(
                topFrameForDisplay,
                iframeForDisplay,
                idpForDisplay,
                accounts,
                idpMetadata,
                clientMetadata,
                isAutoReauthn,
                rpContext);
        setComponentShowTime(SystemClock.elapsedRealtime());
        showBrandIcon(idpMetadata);
    }

    void showFailureDialog(
            String topFrameForDisplay,
            String iframeForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            String rpContext) {
        showPlaceholderIcon(idpMetadata);
        mTopFrameForDisplay = topFrameForDisplay;
        mIframeForDisplay = iframeForDisplay;
        mIdpForDisplay = idpForDisplay;
        mIdpMetadata = idpMetadata;
        mRpContext = rpContext;
        mHeaderType = HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC;
        updateSheet(/* accounts= */ null, /* areAccountsClickable= */ false);
        setComponentShowTime(SystemClock.elapsedRealtime());
        showBrandIcon(idpMetadata);
    }

    void showErrorDialog(
            String topFrameForDisplay,
            String iframeForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            String rpContext,
            IdentityCredentialTokenError error) {
        showPlaceholderIcon(idpMetadata);
        mTopFrameForDisplay = topFrameForDisplay;
        mIframeForDisplay = iframeForDisplay;
        mIdpForDisplay = idpForDisplay;
        mIdpMetadata = idpMetadata;
        mRpContext = rpContext;
        mError = error;
        mHeaderType = HeaderProperties.HeaderType.SIGN_IN_ERROR;
        updateSheet(/* accounts= */ null, /* areAccountsClickable= */ false);
        setComponentShowTime(SystemClock.elapsedRealtime());
        showBrandIcon(idpMetadata);
    }

    @VisibleForTesting
    void setComponentShowTime(long componentShowTime) {
        mComponentShowTime = componentShowTime;
    }

    @VisibleForTesting
    KeyboardVisibilityListener getKeyboardEventListener() {
        return mKeyboardVisibilityListener;
    }

    @VisibleForTesting
    TabObserver getTabObserver() {
        return mTabObserver;
    }

    private void showAccountsInternal(
            String topFrameForDisplay,
            String iframeForDisplay,
            String idpForDisplay,
            List<Account> accounts,
            IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata,
            boolean isAutoReauthn,
            String rpContext) {
        mTopFrameForDisplay = topFrameForDisplay;
        mIframeForDisplay = iframeForDisplay;
        mIdpForDisplay = idpForDisplay;
        mAccounts = accounts;
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;
        mRpContext = rpContext;

        if (mSelectedAccount != null) {
            accounts = Arrays.asList(mSelectedAccount);
        }

        mHeaderType = isAutoReauthn ? HeaderType.VERIFY_AUTO_REAUTHN : HeaderType.SIGN_IN;
        updateSheet(accounts, /* areAccountsClickable= */ mSelectedAccount == null);
        updateBackPressBehavior();
    }

    private void updateSheet(List<Account> accounts, boolean areAccountsClickable) {
        updateAccounts(mIdpForDisplay, accounts, areAccountsClickable);
        updateHeader();

        boolean isDataSharingConsentVisible = false;
        Callback<Account> continueButtonCallback = null;
        if (mHeaderType == HeaderType.SIGN_IN && mSelectedAccount != null) {
            // Only show the user data sharing consent text for sign up.
            isDataSharingConsentVisible = !mSelectedAccount.isSignIn();
            continueButtonCallback = this::onClickAccountSelected;
        }

        if (mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN) {
            assert mSelectedAccount != null;
            assert mSelectedAccount.isSignIn();

            onAccountSelected(mSelectedAccount);
        }

        if (mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            assert !isDataSharingConsentVisible;
            assert mSelectedAccount == null;
            continueButtonCallback = this::onLoginToIdP;
        }

        if (mHeaderType == HeaderType.SIGN_IN_ERROR) {
            assert !isDataSharingConsentVisible;
            continueButtonCallback = this::onClickGotItButton;
        }

        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                (continueButtonCallback != null)
                        ? createContinueBtnItem(
                                mSelectedAccount, mIdpMetadata, continueButtonCallback)
                        : null);
        mModel.set(
                ItemProperties.DATA_SHARING_CONSENT,
                isDataSharingConsentVisible
                        ? createDataSharingConsentItem(mIdpForDisplay, mClientMetadata)
                        : null);
        mModel.set(
                ItemProperties.IDP_SIGNIN,
                mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC
                        ? createIdpSignInItem(mIdpForDisplay)
                        : null);
        mModel.set(
                ItemProperties.ERROR_TEXT,
                mHeaderType == HeaderType.SIGN_IN_ERROR
                        ? createErrorTextItem(mIdpForDisplay, mTopFrameForDisplay, mError)
                        : null);

        mBottomSheetContent.computeAndUpdateAccountListHeight();
        showContent();
    }

    private void updateHeader() {
        PropertyModel headerModel =
                createHeaderItem(
                        mHeaderType,
                        mTopFrameForDisplay,
                        mIframeForDisplay,
                        mIdpForDisplay,
                        mRpContext);
        mModel.set(ItemProperties.HEADER, headerModel);
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    private void showContent() {
        if (mWasDismissed) return;
        if (mIsModalDialogOpen) return;
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            if (mRegisteredObservers) return;

            mRegisteredObservers = true;
            mBottomSheetController.addObserver(mBottomSheetObserver);
            KeyboardVisibilityDelegate.getInstance()
                    .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
            mTab.addObserver(mTabObserver);
        } else {
            onDismissed(IdentityRequestDialogDismissReason.OTHER);
        }
    }

    /** Requests to dismiss bottomsheet. */
    void dismissContent() {
        mWasDismissed = true;
        KeyboardVisibilityDelegate.getInstance()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mTab.removeObserver(mTabObserver);
        mBottomSheetController.hideContent(mBottomSheetContent, true);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        updateBackPressBehavior();
    }

    private void requestAvatarImage(PropertyModel accountModel) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final String name = account.getName();
        final String avatarURL = account.getPictureUrl().getSpec();

        if (!avatarURL.isEmpty()) {
            ImageFetcher.Params params =
                    ImageFetcher.Params.create(
                            avatarURL,
                            ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME,
                            mDesiredAvatarSize,
                            mDesiredAvatarSize);

            mImageFetcher.fetchImage(
                    params,
                    bitmap -> {
                        accountModel.set(
                                AccountProperties.AVATAR,
                                new AccountProperties.Avatar(name, bitmap, mDesiredAvatarSize));
                    });
        } else {
            accountModel.set(
                    AccountProperties.AVATAR,
                    new AccountProperties.Avatar(name, null, mDesiredAvatarSize));
        }
    }

    boolean wasDismissed() {
        return mWasDismissed;
    }

    /**
     * Event listener for when the user taps on an account or the continue button of the
     * bottomsheet, when it is an IdP sign-in sheet.
     */
    void onLoginToIdP(Account account) {
        // This method only has an Account to match the type of the event listener.
        assert account == null;
        if (!shouldInputBeProcessed()) return;
        mDelegate.onLoginToIdP(mIdpMetadata.getLoginUrl());
    }

    /** Event listener for when the user taps on the more details button of the bottomsheet. */
    void onMoreDetails() {
        if (!shouldInputBeProcessed()) return;
        mDelegate.onMoreDetails();
        onDismissed(IdentityRequestDialogDismissReason.MORE_DETAILS_BUTTON);
    }

    /**
     * Event listener for when the user taps on an account or the continue button of the
     * bottomsheet.
     *
     * @param selectedAccount is the account that the user tapped on. If the user instead tapped on
     *         the continue button, it is the account displayed if this was the single account
     *         chooser.
     */
    void onClickAccountSelected(Account selectedAccount) {
        if (!shouldInputBeProcessed()) return;
        onAccountSelected(selectedAccount);
    }

    /** Event listener for when the user taps on the got it button of the bottomsheet. */
    void onClickGotItButton(Account account) {
        // This method only has an Account to match the type of the event listener. However, it
        // should be non-null because an account must have been selected in order to reach an error
        // dialog.
        assert account != null;
        if (!shouldInputBeProcessed()) return;
        onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
    }

    void onAccountSelected(Account selectedAccount) {
        if (mWasDismissed) return;

        Account oldSelectedAccount = mSelectedAccount;
        mSelectedAccount = selectedAccount;
        if (oldSelectedAccount == null && !mSelectedAccount.isSignIn()) {
            showAccountsInternal(
                    mTopFrameForDisplay,
                    mIframeForDisplay,
                    mIdpForDisplay,
                    mAccounts,
                    mIdpMetadata,
                    mClientMetadata,
                    /* isAutoReauthn= */ false,
                    mRpContext);
            return;
        }

        mDelegate.onAccountSelected(mIdpMetadata.getConfigUrl(), selectedAccount);
        showVerifySheet(selectedAccount);
        updateBackPressBehavior();
    }

    void onDismissed(@IdentityRequestDialogDismissReason int dismissReason) {
        dismissContent();
        mDelegate.onDismissed(dismissReason);
    }

    private PropertyModel createAccountItem(Account account, boolean isAccountClickable) {
        return new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                .with(AccountProperties.ACCOUNT, account)
                .with(
                        AccountProperties.ON_CLICK_LISTENER,
                        isAccountClickable ? this::onClickAccountSelected : null)
                .build();
    }

    private PropertyModel createContinueBtnItem(
            Account account,
            IdentityProviderMetadata idpMetadata,
            Callback<Account> onClickListener) {
        assert account != null
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_ERROR;

        ContinueButtonProperties.Properties properties = new ContinueButtonProperties.Properties();
        properties.mAccount = account;
        properties.mIdpMetadata = idpMetadata;
        properties.mOnClickListener = onClickListener;
        properties.mHeaderType = mHeaderType;
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createDataSharingConsentItem(
            String idpForDisplay, ClientIdMetadata metadata) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mTermsOfServiceUrl = metadata.getTermsOfServiceUrl();
        properties.mPrivacyPolicyUrl = metadata.getPrivacyPolicyUrl();
        properties.mTermsOfServiceClickRunnable =
                () -> {
                    RecordHistogram.recordBooleanHistogram(
                            "Blink.FedCm.SignUp.TermsOfServiceClicked", true);
                };
        properties.mPrivacyPolicyClickRunnable =
                () -> {
                    RecordHistogram.recordBooleanHistogram(
                            "Blink.FedCm.SignUp.PrivacyPolicyClicked", true);
                };

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createIdpSignInItem(String idpForDisplay) {
        return new PropertyModel.Builder(IdpSignInProperties.ALL_KEYS)
                .with(IdpSignInProperties.IDP_FOR_DISPLAY, idpForDisplay)
                .build();
    }

    private PropertyModel createErrorTextItem(
            String idpForDisplay, String topFrameForDisplay, IdentityCredentialTokenError error) {
        ErrorProperties.Properties properties = new ErrorProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mTopFrameForDisplay = topFrameForDisplay;
        properties.mError = error;
        properties.mMoreDetailsClickRunnable =
                !error.getUrl().isEmpty() ? this::onMoreDetails : null;
        return new PropertyModel.Builder(ErrorProperties.ALL_KEYS)
                .with(ErrorProperties.PROPERTIES, properties)
                .build();
    }

    void onModalDialogOpened() {
        mIsModalDialogOpen = true;
    }

    void onModalDialogClosed() {
        mIsModalDialogOpen = false;
    }
}

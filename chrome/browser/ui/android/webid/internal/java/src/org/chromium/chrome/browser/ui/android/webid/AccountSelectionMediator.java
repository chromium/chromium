// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.LoginButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content.webid.IdentityRequestDialogLinkType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mRegisteredObservers;
    private boolean mWasDismissed;
    // Keeps track of the last bottom sheet seen by the BottomSheetObserver. Used to know whether a
    // sheet state change affects the BottomSheet owned by this object or not.
    private BottomSheetContent mLastSheetSeen;
    @VisibleForTesting private final Tab mTab;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final PropertyModel mModel;
    private final ModelList mSheetAccountItems;
    private final @Px int mDesiredAvatarSize;
    private final @RpMode.EnumType int mRpMode;

    private final BottomSheetController mBottomSheetController;
    private final AccountSelectionBottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final TabObserver mTabObserver;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;

    // Amount of time during which we ignore inputs. Note that this is timed from when we invoke the
    // methods to show the accounts, so it does include any time spent animating the sheet into
    // view.
    public static final long POTENTIALLY_UNINTENDED_INPUT_THRESHOLD = 500;

    private HeaderType mHeaderType;
    private String mRpForDisplay;
    private String mIdpForDisplay;
    // The icon to be displayed in the title of the dialog. Corresponds to the IDP icon when one IDP
    // is involved or the RP icon when multiple IDPs are involved.
    private Bitmap mHeaderIcon;
    // The RP brand icon provided by the IDP. Used only in active mode.
    private Bitmap mRpBrandIcon;
    private @RpContext.EnumType int mRpContext;
    private IdentityCredentialTokenError mError;
    private UkmRecorder mUkmRecorder;

    // All of the user's accounts. Populated only when showAccounts() is invoked.
    private List<Account> mAccounts;

    // All of the IDPs involved in the dialog. Populated only when showAccounts() is invoked, as
    // otherwise `mIdpMetadataForLoginOrError` is used.
    private List<IdentityProviderData> mIdpDataListForShowAccounts;

    // The metadata of the IDP involved in the dialog when we show something different from
    // showAccounts(): mismatch UI or error dialog.
    private IdentityProviderMetadata mIdpMetadataForLoginOrError;

    // Whether the current state of the dialog involves multiple IDPs.
    private boolean mIsMultipleIdps;

    // The account that the user has selected.
    private Account mSelectedAccount;

    // Stores the value of SystemClock.elapsedRealtime() at the time in which the accounts are shown
    // to the user.
    private long mComponentShowTime;

    // Whether there is an open modal dialog. When a modal dialog is opened, this
    // mediator should not display any accounts until such dialog is closed.
    private boolean mIsModalDialogOpen;

    // View to explicitly focus on for screen reader accessibility purposes.
    private View mFocusView;

    // The current state of the account chooser if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mAccountChooserState;

    // The current state of the loading dialog if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mLoadingDialogState;

    // The current state of the disclosure dialog if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mDisclosureDialogState;

    // Whether there was a login to IDP CCT that was closed while the loading dialog is open. This
    // could mean that the user successfully completed the login to IDP flow.
    private boolean mIsLoadingDialogLoginToIdpClosed;

    private final KeyboardVisibilityListener mKeyboardVisibilityListener =
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
            @Px int desiredAvatarSize,
            @RpMode.EnumType int rpMode,
            Context context,
            ModalDialogManager modalDialogManager) {
        assert tab != null;
        mTab = tab;
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mSheetAccountItems = sheetAccountItems;
        mDesiredAvatarSize = desiredAvatarSize;
        mRpMode = rpMode;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mLastSheetSeen = mBottomSheetContent;
        if (mTab != null && mTab.getWebContents() != null) {
            mUkmRecorder = new UkmRecorder(mTab.getWebContents(), "Blink.FedCm");
        }

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    // Sends focus events to the relevant views for accessibility.
                    // TODO(crbug.com/40262629): Add tests for TalkBack on FedCM.
                    private void focusForAccessibility() {
                        View contentView =
                                mBottomSheetController.getCurrentSheetContent().getContentView();
                        assert contentView != null;
                        View continueButton =
                                contentView.findViewById(R.id.account_selection_continue_btn);

                        boolean isSingleAccountChooser =
                                mAccounts != null
                                        && mAccounts.size() == 1
                                        && !mAccounts.get(0).isFilteredOut();
                        View focusView =
                                mFocusView != null
                                        ? mFocusView
                                        : continueButton != null
                                                        && continueButton.isShown()
                                                        && !isSingleAccountChooser
                                                        && getSheetType()
                                                                == SheetType.ACCOUNT_SELECTION
                                                ? continueButton
                                                : contentView.findViewById(R.id.header);

                        if (focusView == null) return;

                        focusView.requestFocus();
                        focusView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                        mFocusView = null;
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
                                int dismissReason = IdentityRequestDialogDismissReason.OTHER;
                                if (reason == BottomSheetController.StateChangeReason.SWIPE) {
                                    dismissReason = IdentityRequestDialogDismissReason.SWIPE;
                                    // mAccountChooserState is not null only if we want to record
                                    // this metric, that is, a active mode explicit sign-in account
                                    // chooser was shown.
                                    mAccountChooserState =
                                            mAccountChooserState != null
                                                    ? AccountChooserResult.SWIPE
                                                    : null;
                                    mLoadingDialogState =
                                            mHeaderType == HeaderType.LOADING
                                                    ? LoadingDialogResult.SWIPE
                                                    : null;
                                    mDisclosureDialogState =
                                            mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                                                    ? DisclosureDialogResult.SWIPE
                                                    : null;
                                } else if (reason
                                        == BottomSheetController.StateChangeReason.BACK_PRESS) {
                                    dismissReason = IdentityRequestDialogDismissReason.BACK_PRESS;
                                    // mAccountChooserState is not null only if we want to record
                                    // this metric, that is, a active mode explicit sign-in account
                                    // chooser was shown.
                                    mAccountChooserState =
                                            mAccountChooserState != null
                                                    ? AccountChooserResult.BACK_PRESS
                                                    : null;
                                    mLoadingDialogState =
                                            mHeaderType == HeaderType.LOADING
                                                    ? LoadingDialogResult.BACK_PRESS
                                                    : null;
                                } else if (reason
                                        == BottomSheetController.StateChangeReason.TAP_SCRIM) {
                                    dismissReason = IdentityRequestDialogDismissReason.TAP_SCRIM;
                                    // mAccountChooserState is not null only if we want to record
                                    // this metric, that is, a active mode explicit sign-in account
                                    // chooser was shown.
                                    mAccountChooserState =
                                            mAccountChooserState != null
                                                    ? AccountChooserResult.TAP_SCRIM
                                                    : null;
                                    mLoadingDialogState =
                                            mHeaderType == HeaderType.LOADING
                                                    ? LoadingDialogResult.TAP_SCRIM
                                                    : null;
                                    mDisclosureDialogState =
                                            mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                                                    ? DisclosureDialogResult.TAP_SCRIM
                                                    : null;
                                }
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

    private void setFocusView(View focusView) {
        // If focus view has already been set, we do not override it. This is because we bind views
        // in order of most important to least important for accessibility so the first call to this
        // method should be from the most important element that should take focus.
        if (mFocusView != null) return;

        mFocusView = focusView;
    }

    private void updateBackPressBehavior() {
        mBottomSheetContent.setCustomBackPressBehavior(
                !mWasDismissed
                                && ((mSelectedAccount != null
                                                && mAccounts.size() != 1
                                                && mHeaderType != HeaderType.VERIFY)
                                        || mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL)
                        ? this::handleBackPress
                        : null);
    }

    private void handleBackPress() {
        if (mWasDismissed) {
            return;
        }
        mSelectedAccount = null;
        if (mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL) {
            mDisclosureDialogState = DisclosureDialogResult.BACK_PRESS;
            maybeRecordDisclosureDialogResult();
        }
        showAccountsInternal(/* newAccounts= */ null);
    }

    private PropertyModel createHeaderItem(
            HeaderType headerType,
            String rpForDisplay,
            String idpForDisplay,
            @RpContext.EnumType int rpContext,
            Boolean isMultipleIdps) {
        Runnable closeOnClickRunnable =
                () -> {
                    onDismissed(IdentityRequestDialogDismissReason.CLOSE_BUTTON);

                    RecordHistogram.recordBooleanHistogram(
                            "Blink.FedCm.CloseVerifySheet.Android",
                            mHeaderType == HeaderType.VERIFY);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Blink.FedCm.ClosedSheetType.Android", getSheetType(), SheetType.COUNT);
                };

        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.HEADER_ICON, mHeaderIcon)
                .with(HeaderProperties.RP_BRAND_ICON, mRpBrandIcon)
                .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                .with(HeaderProperties.IDP_FOR_DISPLAY, idpForDisplay)
                .with(HeaderProperties.RP_FOR_DISPLAY, rpForDisplay)
                .with(HeaderProperties.TYPE, headerType)
                .with(HeaderProperties.RP_CONTEXT, rpContext)
                .with(HeaderProperties.RP_MODE, mRpMode)
                .with(
                        HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER,
                        mSelectedAccount == null && mAccounts != null && mAccounts.size() > 1)
                .with(HeaderProperties.SET_FOCUS_VIEW_CALLBACK, this::setFocusView)
                .with(HeaderProperties.IS_MULTIPLE_IDPS, isMultipleIdps)
                .build();
    }

    private int getSheetType() {
        switch (mHeaderType) {
            case SIGN_IN:
            case REQUEST_PERMISSION_MODAL:
                return SheetType.ACCOUNT_SELECTION;
            case VERIFY:
                return SheetType.VERIFYING;
            case VERIFY_AUTO_REAUTHN:
                return SheetType.AUTO_REAUTHN;
            case SIGN_IN_TO_IDP_STATIC:
                return SheetType.SIGN_IN_TO_IDP_STATIC;
            case SIGN_IN_ERROR:
                return SheetType.SIGN_IN_ERROR;
            case LOADING:
                return SheetType.LOADING;
        }
        assert false; // NOTREACHED
        return SheetType.ACCOUNT_SELECTION;
    }

    private void updateAccounts(
            List<Account> accounts,
            List<IdentityProviderData> identityProviders,
            boolean areAccountsClickable,
            boolean skipAddAccountRows) {
        mSheetAccountItems.clear();
        // In the request permission dialog, account is shown as an account chip instead of in the
        // accounts list. In the active mode verifying dialog, we do not show accounts.
        if (mRpMode == RpMode.ACTIVE
                && (mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                        || mHeaderType == HeaderType.VERIFY
                        || mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN)) {
            return;
        }

        for (Account account : accounts) {
            final PropertyModel model = createAccountItem(account, areAccountsClickable);
            mSheetAccountItems.add(
                    new ListItem(AccountSelectionProperties.ITEM_TYPE_ACCOUNT, model));
        }

        if (skipAddAccountRows) {
            return;
        }
        boolean hasAddedLoginButton = false;
        for (IdentityProviderData identityProvider : identityProviders) {
            if (identityProvider.getIdpMetadata().showUseDifferentAccountButton()
                    || identityProvider.hasLoginStatusMismatch()) {
                if (!hasAddedLoginButton && !accounts.isEmpty() && mRpMode == RpMode.PASSIVE) {
                    mSheetAccountItems.add(
                            new ListItem(
                                    AccountSelectionProperties.ITEM_TYPE_SEPARATOR,
                                    new PropertyModel()));
                }
                final PropertyModel model = createLoginBtnItem(identityProvider);
                mSheetAccountItems.add(
                        new ListItem(AccountSelectionProperties.ITEM_TYPE_LOGIN, model));
                hasAddedLoginButton = true;
            }
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

    private boolean isValidBrandIcon(Bitmap bitmap, boolean shouldCircleCrop) {
        return bitmap != null
                && bitmap.getWidth() == bitmap.getHeight()
                && (!shouldCircleCrop
                        || bitmap.getWidth()
                                >= AccountSelectionBridge.getBrandIconMinimumSize(mRpMode));
    }

    private void updateHeaderIcon(Bitmap bitmap, boolean shouldCircleCrop) {
        mHeaderIcon = isValidBrandIcon(bitmap, shouldCircleCrop) ? bitmap : null;
    }

    private void updateRpBrandIcon(Bitmap bitmap) {
        mRpBrandIcon = isValidBrandIcon(bitmap, /* shouldCircleCrop= */ true) ? bitmap : null;
    }

    private void maybeRecordAccountChooserResult(int result) {
        if (mAccountChooserState == null) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.Button.AccountChooserResult", result, SheetType.COUNT);
        if (mUkmRecorder != null) {
            mUkmRecorder.addMetric("Button.AccountChooserResult", result).record();
        }
        mAccountChooserState = null;
    }

    private void maybeRecordLoadingDialogResult() {
        if (mRpMode != RpMode.ACTIVE) return;

        // mLoadingDialogState is set on dismissal e.g. tap scrim, back press or if the loading
        // dialog gets updated into an account chooser. If it hasn't been set but onDismissed is
        // called while the loading dialog is being shown, it means the user has closed the tab or
        // navigated away.
        if (mLoadingDialogState == null && mHeaderType == HeaderType.LOADING) {
            mLoadingDialogState = LoadingDialogResult.DESTROY;
        }

        // Can be null in tests that do not show the loading dialog before the dialog being tested.
        if (mLoadingDialogState == null) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.Button.LoadingDialogResult", mLoadingDialogState, SheetType.COUNT);
        if (mUkmRecorder != null) {
            mUkmRecorder.addMetric("Button.LoadingDialogResult", mLoadingDialogState).record();
        }
        mLoadingDialogState = null;
    }

    private void maybeRecordDisclosureDialogResult() {
        if (mRpMode != RpMode.ACTIVE) return;

        // mDisclosureDialogState is set on dismissal e.g. tap scrim, back press or if the user
        // presses continue. If it hasn't been set but onDismissed is called while the disclosure
        // dialog is being shown, it means the user has closed the tab or navigated away.
        if (mDisclosureDialogState == null && mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL) {
            mDisclosureDialogState = DisclosureDialogResult.DESTROY;
        }

        // Can be null when the disclosure dialog is not shown.
        if (mDisclosureDialogState == null) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.Button.DisclosureDialogResult",
                mDisclosureDialogState,
                SheetType.COUNT);
        if (mUkmRecorder != null) {
            mUkmRecorder
                    .addMetric("Button.DisclosureDialogResult", mDisclosureDialogState)
                    .record();
        }
        mDisclosureDialogState = null;
    }

    boolean showVerifySheet(Account account) {
        if (mHeaderType == HeaderType.SIGN_IN
                || mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL) {
            if (mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL) {
                mDisclosureDialogState = DisclosureDialogResult.CONTINUE;
                maybeRecordDisclosureDialogResult();
            }
            mHeaderType = HeaderType.VERIFY;
            if (!updateSheet(
                    Arrays.asList(account),
                    /* identityProviders= */ Collections.emptyList(),
                    /* areAccountsClickable= */ false)) {
                return false;
            }
            updateBackPressBehavior();
        } else {
            assert mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN;
            // Auto reauthn is triggered without showAccounts so we need to update the sheet
            // explicitly.
            if (!updateSheet(
                    Arrays.asList(account),
                    /* identityProviders= */ Collections.emptyList(),
                    /* areAccountsClickable= */ false)) {
                return false;
            }
        }
        return true;
    }

    boolean showRequestPermissionModalSheet(Account account) {
        mHeaderType = HeaderType.REQUEST_PERMISSION_MODAL;
        if (!updateSheet(
                Arrays.asList(account),
                /* identityProviders= */ Collections.emptyList(),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        updateBackPressBehavior();
        return true;
    }

    // Dismisses content without notifying the delegate. Should only be invoked during destruction.
    void close() {
        if (!mWasDismissed) dismissContent();
    }

    boolean showAccounts(
            String rpForDisplay,
            List<Account> accounts,
            List<IdentityProviderData> idpDataList,
            List<Account> newAccounts) {
        if (mWasDismissed) {
            return false;
        }
        mRpForDisplay = rpForDisplay;
        mAccounts = accounts;
        mIdpDataListForShowAccounts = idpDataList;
        mIdpMetadataForLoginOrError = null;
        setIsMultipleIdps(mIdpDataListForShowAccounts.size() > 1);
        mRpContext = mIdpDataListForShowAccounts.get(0).getRpContext();
        mSelectedAccount = null;
        mLoadingDialogState =
                mIsLoadingDialogLoginToIdpClosed
                        ? LoadingDialogResult.PROCEED_THROUGH_POPUP
                        : LoadingDialogResult.PROCEED;
        maybeRecordLoadingDialogResult();

        if (accounts.size() == 1) {
            // An account is automatically 'selected' if that is the
            // only option.
            if (!mIsMultipleIdps
                    && !mIdpDataListForShowAccounts
                            .get(0)
                            .getIdpMetadata()
                            .showUseDifferentAccountButton()) {
                mSelectedAccount = accounts.get(0);
            }
        }

        if (!showAccountsInternal(newAccounts)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        return true;
    }

    boolean showFailureDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext) {
        mRpForDisplay = rpForDisplay;
        mIdpForDisplay = idpForDisplay;
        mIdpMetadataForLoginOrError = idpMetadata;
        mIdpDataListForShowAccounts = null;
        mRpContext = rpContext;
        mHeaderType = HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC;
        if (!updateSheet(
                /* accounts= */ Collections.emptyList(),
                /* identityProviders= */ Collections.emptyList(),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        updateHeaderIcon(idpMetadata.getBrandIconBitmap(), /* shouldCircleCrop= */ true);
        updateHeader();
        return true;
    }

    boolean showErrorDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext,
            IdentityCredentialTokenError error) {
        mRpForDisplay = rpForDisplay;
        mIdpForDisplay = idpForDisplay;
        mIdpMetadataForLoginOrError = idpMetadata;
        mIdpDataListForShowAccounts = null;
        mRpContext = rpContext;
        mError = error;
        mHeaderType = HeaderProperties.HeaderType.SIGN_IN_ERROR;
        setComponentShowTime(SystemClock.elapsedRealtime());

        // Update the bottom sheet into an error bottom sheet for passive mode.
        if (mRpMode == RpMode.PASSIVE) {
            if (!updateSheet(
                    /* accounts= */ Collections.emptyList(),
                    /* identityProviders= */ Collections.emptyList(),
                    /* areAccountsClickable= */ false)) {
                return false;
            }
            updateHeaderIcon(idpMetadata.getBrandIconBitmap(), /* shouldCircleCrop= */ true);
            updateHeader();
            return true;
        }

        // Hide the bottom sheet and show an error modal dialog for active mode.
        mBottomSheetController.hideContent(mBottomSheetContent, false);
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        dismissalCause -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                onClickGotItButton(
                                        new ButtonData(mSelectedAccount, /* idpMetadata= */ null));
                            } else if (dismissalCause
                                    == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                                onMoreDetails();
                            } else {
                                onDismissed(IdentityRequestDialogDismissReason.OTHER);
                            }
                        });
        ErrorProperties.Properties properties = new ErrorProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mRpForDisplay = rpForDisplay;
        properties.mError = error;
        properties.mMoreDetailsClickRunnable =
                !error.getUrl().isEmpty() ? this::onMoreDetails : null;
        AccountSelectionViewBinder.ErrorText errorText =
                AccountSelectionViewBinder.getErrorText(
                        mContext, properties, /* clickableText= */ false);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(ModalDialogProperties.TITLE, errorText.mSummary)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, errorText.mDescription)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(R.string.signin_error_dialog_got_it_button));
        if (!error.getUrl().isEmpty()) {
            builder.with(
                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                            mContext.getString(R.string.signin_error_dialog_more_details_button))
                    .with(
                            ModalDialogProperties.BUTTON_STYLES,
                            ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
        } else {
            builder.with(
                    ModalDialogProperties.BUTTON_STYLES,
                    ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE);
        }
        PropertyModel model = builder.build();
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
        return true;
    }

    boolean showLoadingDialog(
            String rpForDisplay, String idpForDisplay, @RpContext.EnumType int rpContext) {
        mRpForDisplay = rpForDisplay;
        mIdpForDisplay = idpForDisplay;
        mRpContext = rpContext;
        mHeaderType = HeaderProperties.HeaderType.LOADING;
        if (!updateSheet(
                /* accounts= */ Collections.emptyList(),
                /* identityProviders= */ Collections.emptyList(),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        return true;
    }

    boolean showVerifyingDialog(Account account, boolean isAutoReauthn) {
        mHeaderType = isAutoReauthn ? HeaderType.VERIFY_AUTO_REAUTHN : HeaderType.VERIFY;
        mSelectedAccount = account;

        // Auto re-authn in active mode does not update the loading UI.
        if (mRpMode == RpMode.ACTIVE && isAutoReauthn) {
            return true;
        }

        showVerifySheet(mSelectedAccount);
        return true;
    }

    void showUrl(Context context, @IdentityRequestDialogLinkType int linkType, GURL url) {
        switch (linkType) {
            case IdentityRequestDialogLinkType.TERMS_OF_SERVICE:
                RecordHistogram.recordBooleanHistogram(
                        "Blink.FedCm.SignUp.TermsOfServiceClicked", true);
                break;
            case IdentityRequestDialogLinkType.PRIVACY_POLICY:
                RecordHistogram.recordBooleanHistogram(
                        "Blink.FedCm.SignUp.PrivacyPolicyClicked", true);
                break;
        }
        CustomTabActivity.showInfoPage(context, url.getSpec());
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

    @VisibleForTesting
    HeaderType getHeaderType() {
        return mHeaderType;
    }

    private boolean showAccountsInternal(@Nullable List<Account> newAccounts) {
        // TODO(crbug.com/356665527): Handle multiple newly signed-in accounts.
        Account newlySignedInAccount =
                newAccounts != null
                                && newAccounts.size() == 1
                                && !newAccounts.get(0).isFilteredOut()
                        ? newAccounts.get(0)
                        : null;

        if (newlySignedInAccount != null && mRpMode == RpMode.ACTIVE) {
            mSelectedAccount = newlySignedInAccount;

            // The browser trusted login state controls whether we'd skip the next
            // dialog. One caveat: if a user was logged out of the IdP and they just
            // logged in with a returning account from the LOADING state, we do not
            // skip the next UI when mediation mode is `required` because there was
            // not user mediation acquired yet in this case.
            boolean shouldShowVerifyingSheet =
                    newlySignedInAccount.isBrowserTrustedSignIn()
                            && mHeaderType != HeaderType.LOADING;
            if (shouldShowVerifyingSheet) {
                mHeaderType = HeaderType.SIGN_IN;
                mDelegate.onAccountSelected(mSelectedAccount);
                return showVerifySheet(mSelectedAccount);
            }

            // The IDP claimed login state controls whether we show disclosure text,
            // if we do not skip the next dialog. Also skip when request_permission
            // is false (controlled by the fields API).
            boolean shouldShowRequestPermissionDialog =
                    !newlySignedInAccount.isSignIn()
                            && newlySignedInAccount
                                            .getIdentityProviderData()
                                            .getDisclosureFields()
                                            .length
                                    > 0;
            if (shouldShowRequestPermissionDialog) {
                return showRequestPermissionModalSheet(mSelectedAccount);
            }

            // Else:
            // Show accounts picker which doesn't contain the disclosure text. We do not support
            // request permission UI without disclosure text.
        }

        mHeaderType = HeaderType.SIGN_IN;
        // Show the selected account's IDP or only show the IDP if there is just one.
        if (mSelectedAccount != null) {
            mIdpForDisplay = mSelectedAccount.getIdentityProviderData().getIdpForDisplay();
        } else if (mIdpDataListForShowAccounts.size() == 1) {
            mIdpForDisplay = mIdpDataListForShowAccounts.get(0).getIdpForDisplay();
        } else {
            mIdpForDisplay = null;
        }

        // We want the accounts to be clickable if there is no preselected account or if we're not
        // going to show the disclosure text, which happens when the account is a signIn or when
        // fields is empty.
        if (!updateSheet(
                mSelectedAccount != null ? Arrays.asList(mSelectedAccount) : mAccounts,
                mSelectedAccount != null ? Collections.emptyList() : mIdpDataListForShowAccounts,
                /* areAccountsClickable= */ mSelectedAccount == null
                        || mSelectedAccount.isSignIn()
                        || mSelectedAccount.getIdentityProviderData().getDisclosureFields().length
                                == 0)) {
            return false;
        }
        updateBackPressBehavior();

        // This is a placeholder assuming the tab containing the account chooser will be closed.
        // This will be updated upon user action i.e. clicking on account row, use
        // other account button or swiped down. If we do not receive any of these actions by time
        // onDismissed() is called, it means our placeholder assumption is true i.e. the user has
        // closed the tab.
        if (mRpMode == RpMode.ACTIVE) {
            // If there was already an account chooser state from a previously shown account
            // chooser, record the outcome and reset the state.
            if (mAccountChooserState != null) {
                maybeRecordAccountChooserResult(mAccountChooserState);
            }
            mAccountChooserState = AccountChooserResult.TAB_CLOSED;
        }
        return true;
    }

    // Update the sheet so that it includes the provided accounts and identity providers. The
    // identity providers are needed for use other account as well as for mismatch UI. Note that
    // these may be empty lists, depending on the UI being displayed (loading UI, error, etc.). This
    // is determined by querying the mHeaderType, which must be up to date.
    private boolean updateSheet(
            List<Account> accounts,
            List<IdentityProviderData> identityProviders,
            boolean areAccountsClickable) {
        IdentityProviderData firstAccountIdp =
                !accounts.isEmpty() ? accounts.get(0).getIdentityProviderData() : null;

        boolean isSingleAccountChooser = accounts.size() == 1 && !accounts.get(0).isFilteredOut();

        // Check everything we need to render to determine if multiple IDPs are involved or not.
        Set<IdentityProviderData> distinctIdps =
                new HashSet<IdentityProviderData>(identityProviders);
        for (Account account : accounts) {
            distinctIdps.add(account.getIdentityProviderData());
        }
        setIsMultipleIdps(distinctIdps.size() > 1);
        IdentityProviderData uniqueIdp =
                distinctIdps.size() == 1 ? distinctIdps.iterator().next() : null;

        boolean showUseDifferentAccountInSingleAccountChooserActiveMode =
                isSingleAccountChooser
                        && mRpMode == RpMode.ACTIVE
                        && mHeaderType == HeaderType.SIGN_IN
                        && areAccountsClickable
                        && firstAccountIdp != null
                        && firstAccountIdp.getIdpMetadata().showUseDifferentAccountButton();
        // We add the add account button alongside the accounts if supported in passive mode and in
        // the multi-account UI of active mode.
        updateAccounts(
                accounts,
                identityProviders,
                areAccountsClickable,
                showUseDifferentAccountInSingleAccountChooserActiveMode);
        if (uniqueIdp != null) {
            updateHeaderIcon(
                    uniqueIdp.getIdpMetadata().getBrandIconBitmap(), /* shouldCircleCrop= */ true);
        } else {
            updateHeaderIcon(
                    mIsMultipleIdps ? TabFavicon.getBitmap(mTab) : null,
                    /* shouldCircleCrop= */ false);
        }
        assert mRpMode == RpMode.PASSIVE || !mIsMultipleIdps;
        // RP brand icon is set here, but only shown during the request permission dialog.
        if (mRpMode == RpMode.ACTIVE && mIdpDataListForShowAccounts != null) {
            updateRpBrandIcon(
                    mIdpDataListForShowAccounts.get(0).getClientMetadata().getBrandIconBitmap());
        }
        // If there is a change in the header, setFocusView() will be called and focus will land on
        // the header when screen reader is on. Since the header is updated before any item is
        // created, the header will always take precedence for focus. Do not reorder this
        // updateHeader() call to happen after item creation.
        updateHeader();

        boolean isDataSharingConsentVisible = false;
        Callback<ButtonData> continueButtonCallback = null;
        if (mHeaderType == HeaderType.SIGN_IN && mSelectedAccount != null) {
            // Only show the user data sharing consent text for sign up and only
            // if we're asked to request permission.
            isDataSharingConsentVisible =
                    !mSelectedAccount.isSignIn()
                            && mSelectedAccount
                                            .getIdentityProviderData()
                                            .getDisclosureFields()
                                            .length
                                    > 0;
            continueButtonCallback = this::onClickAccountSelected;
        }

        if (mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN) {
            assert mSelectedAccount != null;
            assert mSelectedAccount.isSignIn();
        }

        if (mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            assert !isDataSharingConsentVisible;
            assert mSelectedAccount == null;
            continueButtonCallback = this::onLoginToIdP;
        }

        if (showUseDifferentAccountInSingleAccountChooserActiveMode) {
            assert !isDataSharingConsentVisible;
            mSelectedAccount = accounts.get(0);
            continueButtonCallback = this::onClickAccountSelected;
        }

        if (mHeaderType == HeaderType.SIGN_IN_ERROR) {
            assert !isDataSharingConsentVisible;
            continueButtonCallback = this::onClickGotItButton;
        }

        if (mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL) {
            assert mSelectedAccount != null;
            isDataSharingConsentVisible = true;
            continueButtonCallback = this::onClickAccountSelected;
        }

        // On active mode since the disclosure text is above the continue button, create the
        // disclosure text before creating the continue button so setFocusView() will focus
        // in logical linear reading order. Keep the order in mind when adding an item that calls
        // setFocusView() because the first item which calls it will get the focus.
        if (mRpMode == RpMode.ACTIVE) {
            mModel.set(
                    ItemProperties.DATA_SHARING_CONSENT,
                    isDataSharingConsentVisible
                            ? createDataSharingConsentItem(
                                    mIdpForDisplay,
                                    mSelectedAccount.getIdentityProviderData().getClientMetadata(),
                                    mSelectedAccount
                                            .getIdentityProviderData()
                                            .getDisclosureFields())
                            : null);
        }
        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                (continueButtonCallback != null)
                        ? createContinueBtnItem(
                                mSelectedAccount,
                                mSelectedAccount != null
                                        ? mSelectedAccount
                                                .getIdentityProviderData()
                                                .getIdpMetadata()
                                        : mIdpMetadataForLoginOrError,
                                continueButtonCallback)
                        : null);
        // On passive mode since the disclosure text is below the continue button, create the
        // disclosure text after creating the continue button so setFocusView() will focus
        // in logical linear reading order. Keep the order in mind when adding an item that calls
        // setFocusView() because the first item which calls it will get the focus.
        if (mRpMode == RpMode.PASSIVE) {
            mModel.set(
                    ItemProperties.DATA_SHARING_CONSENT,
                    isDataSharingConsentVisible
                            ? createDataSharingConsentItem(
                                    mIdpForDisplay,
                                    mSelectedAccount.getIdentityProviderData().getClientMetadata(),
                                    mSelectedAccount
                                            .getIdentityProviderData()
                                            .getDisclosureFields())
                            : null);
        }
        mModel.set(
                ItemProperties.IDP_SIGNIN,
                mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC
                        ? createIdpSignInItem(mIdpForDisplay)
                        : null);
        mModel.set(
                ItemProperties.ERROR_TEXT,
                mHeaderType == HeaderType.SIGN_IN_ERROR
                        ? createErrorTextItem(mIdpForDisplay, mRpForDisplay, mError)
                        : null);
        // The add account button is added separately for active mode single account chooser.
        mModel.set(
                ItemProperties.ADD_ACCOUNT_BUTTON,
                showUseDifferentAccountInSingleAccountChooserActiveMode
                        ? createLoginBtnItem(firstAccountIdp)
                        : null);
        mModel.set(
                ItemProperties.ACCOUNT_CHIP,
                mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                        ? createAccountItem(mSelectedAccount, /* isAccountClickable= */ false)
                        : null);
        mModel.set(
                ItemProperties.SPINNER_ENABLED,
                mRpMode == RpMode.ACTIVE
                        && (mHeaderType == HeaderType.LOADING
                                || mHeaderType == HeaderType.VERIFY
                                || mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN));

        // In passive mode, the dragbar visibility depends on whether there are multiple IDPs or
        // not.
        mModel.set(
                ItemProperties.DRAGBAR_HANDLE_VISIBLE, mRpMode == RpMode.ACTIVE || mIsMultipleIdps);

        mBottomSheetController.expandSheet();
        // When a user opens a page that invokes the FedCM API in a new tab, the tab will be hidden
        // and we should not show the bottom sheet to avoid confusion.
        mTab.addObserver(mTabObserver);
        if (!mTab.isHidden()) {
            return showContent();
        }
        return true;
    }

    private void updateHeader() {
        PropertyModel headerModel =
                createHeaderItem(
                        mHeaderType, mRpForDisplay, mIdpForDisplay, mRpContext, mIsMultipleIdps);
        mModel.set(ItemProperties.HEADER, headerModel);
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    private boolean showContent() {
        if (mWasDismissed || mIsModalDialogOpen) {
            return true;
        }
        // When active mode is triggered, if there's a pending passive mode request, we should
        // prioritize the active mode since it's gated by user intention. With the UI code, both
        // button flow bottom sheet and widget flow bottom sheet have the same predefined priority
        // therefore the consecutive button flow would be dismissed. Here we override the
        // calculation and prioritize the button flow request.
        boolean prioritizeActiveMode =
                mRpMode == RpMode.ACTIVE && mHeaderType == HeaderType.LOADING;
        if (!mBottomSheetController.requestShowContent(mBottomSheetContent, true)
                && !prioritizeActiveMode) {
            onDismissed(IdentityRequestDialogDismissReason.OTHER);
            return false;
        }
        if (mRegisteredObservers) return true;

        mRegisteredObservers = true;
        if (mHeaderType == HeaderType.SIGN_IN
                || mHeaderType == HeaderType.VERIFY
                || mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN) {
            mDelegate.onAccountsDisplayed();
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        KeyboardVisibilityDelegate.getInstance()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        if (!mTab.hasObserver(mTabObserver)) mTab.addObserver(mTabObserver);
        return true;
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
        final String displayName = account.getDisplayName();
        final Bitmap picture =
                mIsMultipleIdps
                        ? account.getCircledBadgedPictureBitmap()
                        : account.getPictureBitmap();
        int avatarSize =
                mIsMultipleIdps
                        ? mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.account_selection_account_avatar_multi_idp_size)
                        : mDesiredAvatarSize;
        accountModel.set(
                AccountProperties.AVATAR,
                new AccountProperties.Avatar(displayName, picture, avatarSize));
    }

    private void setIsMultipleIdps(boolean isMultipleIdps) {
        mIsMultipleIdps = isMultipleIdps;
        mBottomSheetContent.setIsMultipleIdps(isMultipleIdps);
    }

    boolean wasDismissed() {
        return mWasDismissed;
    }

    /**
     * Event listener for when the user taps on an account or the continue button of the
     * bottomsheet, when it is an IdP sign-in sheet.
     */
    void onLoginToIdP(ButtonData buttonData) {
        assert buttonData.mIdpMetadata != null;
        assert buttonData.mAccount == null;
        if (!shouldInputBeProcessed()) return;
        maybeRecordAccountChooserResult(AccountChooserResult.USE_OTHER_ACCOUNT_BUTTON);
        mDelegate.onLoginToIdP(
                buttonData.mIdpMetadata.getConfigUrl(), buttonData.mIdpMetadata.getLoginUrl());
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
     * @param buttonData contains the account that the user tapped on. If the user instead tapped on
     *     the continue button, it is the account displayed if this was the single account chooser.
     */
    void onClickAccountSelected(ButtonData buttonData) {
        assert buttonData.mAccount != null;
        if (!shouldInputBeProcessed()) return;
        maybeRecordAccountChooserResult(AccountChooserResult.ACCOUNT_ROW);
        onAccountSelected(buttonData);
    }

    /** Event listener for when the user taps on the got it button of the bottomsheet. */
    void onClickGotItButton(ButtonData buttonData) {
        // This method only has an Account to match the type of the event listener. However, it
        // should be non-null because an account must have been selected in order to reach an error
        // dialog.
        assert buttonData.mAccount != null;
        if (!shouldInputBeProcessed()) return;
        onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
    }

    void onAccountSelected(ButtonData buttonData) {
        assert buttonData.mAccount != null;
        if (mWasDismissed) return;

        // There is an old selected account if an account was already selected from an account
        // chooser and it implies that this `onAccountSelected` call comes from a dialog containing
        // disclosure text or that the user has just signed into a new account on an IDP through
        // FedCM.
        Account oldSelectedAccount = mSelectedAccount;
        mSelectedAccount = buttonData.mAccount;

        // If the account is a returning user or if the account is selected from UI which shows the
        // disclosure text or if the browser doesn't need to request permission because the IDP
        // prefers asking for permission by themselves, skip the disclosure UI and proceed to the
        // verifying sheet.
        if ((mRpMode == RpMode.PASSIVE && oldSelectedAccount != null)
                || mSelectedAccount.isSignIn()
                || mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                || mSelectedAccount.getIdentityProviderData().getDisclosureFields().length == 0) {
            mDelegate.onAccountSelected(mSelectedAccount);
            showVerifySheet(mSelectedAccount);
            return;
        }

        // At this point, the account is a non-returning user. If RP mode is button,
        // we'd request permission through the request permission modal dialog.
        if (mRpMode == RpMode.ACTIVE) {
            showRequestPermissionModalSheet(mSelectedAccount);
            return;
        }

        // At this point, the account is a non-returning user and RP mode is widget.
        showAccountsInternal(/* newAccounts= */ null);
    }

    void onDismissed(@IdentityRequestDialogDismissReason int dismissReason) {
        if (mWasDismissed) {
            return;
        }
        boolean isUseOtherAccountCctDismissed =
                mHeaderType == HeaderType.SIGN_IN && mIsModalDialogOpen;
        // If dismissed from use other account CCT, reshow the accounts dialog.
        if (isUseOtherAccountCctDismissed) {
            mIsModalDialogOpen = false;
            mSelectedAccount = null;
            showAccountsInternal(/* newAccounts= */ null);
            return;
        }

        if (mAccountChooserState != null) {
            maybeRecordAccountChooserResult(mAccountChooserState);
        }
        maybeRecordLoadingDialogResult();
        maybeRecordDisclosureDialogResult();
        dismissContent();
        mDelegate.onDismissed(dismissReason);
    }

    private PropertyModel createAccountItem(Account account, boolean isAccountClickable) {
        PropertyModel model =
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(
                                AccountProperties.ON_CLICK_LISTENER,
                                isAccountClickable && !account.isFilteredOut()
                                        ? this::onClickAccountSelected
                                        : null)
                        .with(AccountProperties.SHOW_IDP, mIsMultipleIdps)
                        .build();
        requestAvatarImage(model);
        return model;
    }

    private PropertyModel createContinueBtnItem(
            Account account,
            IdentityProviderMetadata idpMetadata,
            Callback<ButtonData> onClickListener) {
        assert account != null
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_ERROR
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN;

        ContinueButtonProperties.Properties properties = new ContinueButtonProperties.Properties();
        properties.mAccount = account;
        properties.mIdpMetadata = idpMetadata;
        properties.mOnClickListener = onClickListener;
        properties.mHeaderType = mHeaderType;
        properties.mSetFocusViewCallback = this::setFocusView;
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createLoginBtnItem(IdentityProviderData identityProvider) {
        LoginButtonProperties.Properties properties = new LoginButtonProperties.Properties();
        properties.mIdentityProvider = identityProvider;
        properties.mOnClickListener = this::onLoginToIdP;
        properties.mRpMode = mRpMode;
        properties.mShowIdp = mIsMultipleIdps;
        return new PropertyModel.Builder(LoginButtonProperties.ALL_KEYS)
                .with(LoginButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createDataSharingConsentItem(
            String idpForDisplay,
            ClientIdMetadata metadata,
            @IdentityRequestDialogDisclosureField int[] disclosureFields) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mTermsOfServiceUrl = metadata.getTermsOfServiceUrl();
        properties.mPrivacyPolicyUrl = metadata.getPrivacyPolicyUrl();
        properties.mTermsOfServiceClickCallback =
                (Context context) -> {
                    showUrl(
                            context,
                            IdentityRequestDialogLinkType.TERMS_OF_SERVICE,
                            metadata.getTermsOfServiceUrl());
                };
        properties.mPrivacyPolicyClickCallback =
                (Context context) -> {
                    showUrl(
                            context,
                            IdentityRequestDialogLinkType.PRIVACY_POLICY,
                            metadata.getPrivacyPolicyUrl());
                };
        properties.mSetFocusViewCallback = this::setFocusView;
        properties.mDisclosureFields = disclosureFields;

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
            String idpForDisplay, String rpForDisplay, IdentityCredentialTokenError error) {
        ErrorProperties.Properties properties = new ErrorProperties.Properties();
        properties.mIdpForDisplay = idpForDisplay;
        properties.mRpForDisplay = rpForDisplay;
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
        if (!mIsLoadingDialogLoginToIdpClosed && mHeaderType == HeaderType.LOADING) {
            mIsLoadingDialogLoginToIdpClosed = true;
        }
    }
}

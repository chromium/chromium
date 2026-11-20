// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.LoginButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.RelyingPartyData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetFeatureMap;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content.webid.IdentityRequestDialogLinkType;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.common.ContentFeatures;
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
@NullMarked
class AccountSelectionMediator {
    /** Replaces the passive mode sheet with the active mode one. */
    interface ActiveSheetSwitcher {
        /**
         * Replaces the passive mode sheet with the active mode sheet's request permission dialog
         * for {@code account}, which the user selected in the passive mode sheet.
         */
        void switchToActiveSheetForPermission(
                RelyingPartyData rpData,
                List<Account> accounts,
                List<IdentityProviderData> idpDataList,
                Account account);
    }

    private boolean mRegisteredObservers;
    private boolean mWasDismissed;
    private boolean mCanShowUi = true;
    // Keeps track of the last bottom sheet seen by the BottomSheetObserver.
    // Used to know whether a sheet state change affects the BottomSheet owned
    // by this object or not.
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
    // Replaces the passive mode sheet with the active mode one. Non-null exactly in passive mode.
    private final @Nullable ActiveSheetSwitcher mActiveSheetSwitcher;

    // Amount of time during which we ignore inputs. Note that this is timed from when we invoke the
    // methods to show the accounts, so it does include any time spent animating the sheet into
    // view.
    public static final long POTENTIALLY_UNINTENDED_INPUT_THRESHOLD = 500;

    private @HeaderType int mHeaderType = HeaderType.NONE;
    private @MonotonicNonNull RelyingPartyData mRpData;
    private @Nullable String mIdpForDisplay;
    // The icon to be displayed in the title of the dialog. Corresponds to the IDP icon when one IDP
    // is involved or the RP icon when multiple IDPs are involved in passive mode. Null when
    // multiple IDPs are involved in active mode, where the view binder shows a fallback icon.
    private @Nullable Bitmap mHeaderIcon;
    // The RP brand icon provided by the IDP. Used only in active mode.
    private @Nullable Bitmap mRpBrandIcon;
    private @RpContext.EnumType int mRpContext;
    private @MonotonicNonNull IdentityCredentialTokenError mError;
    private @Nullable UkmRecorder mUkmRecorder;

    // All of the user's accounts. Populated only when `showAccounts()` or
    // `showRequestPermissionFromPassiveSheet()` is invoked.
    private @MonotonicNonNull List<Account> mAccounts;

    // All of the IDPs involved in the dialog. Populated only when `showAccounts()` or
    // `showRequestPermissionFromPassiveSheet()` is invoked, as otherwise
    // `mIdpMetadataForLoginOrError` is used.
    private @Nullable List<IdentityProviderData> mIdpDataListForShowAccounts;

    // The metadata of the IDP involved in the dialog when we show something different from
    // showAccounts(): mismatch UI or error dialog.
    private @Nullable IdentityProviderMetadata mIdpMetadataForLoginOrError;

    // Whether the current state of the dialog involves multiple IDPs: the loading sheet of a
    // multi-IDP request, or a sheet whose accounts and IDPs span more than one IDP (in active mode,
    // this includes IDPs that have no account rows). False once a multi-IDP dialog narrows to a
    // single account's IDP (e.g. for the permission, verifying or error UI).
    private boolean mIsShowingMultipleIdps;

    // Whether this is a multi-IDP request: based on the IDPs requested while the loading sheet is
    // shown, then on those passed to `showAccounts()` or `showRequestPermissionFromPassiveSheet()`.
    // Unlike `mIsShowingMultipleIdps`, it stays set once the dialog narrows to one account's IDP.
    private boolean mIsMultiIdpRequest;

    // The account that the user has selected.
    private @Nullable Account mSelectedAccount;

    // Stores the value of SystemClock.elapsedRealtime() at the time in which the accounts are shown
    // to the user.
    private long mComponentShowTime;

    // Whether there is an open modal dialog. When a modal dialog is opened, this
    // mediator should not display any accounts until such dialog is closed.
    private boolean mIsModalDialogOpen;

    // View to explicitly focus on for screen reader accessibility purposes.
    private @Nullable View mFocusView;

    // The current state of the account chooser if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mAccountChooserState;

    // The current state of the loading dialog if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mLoadingDialogState;

    // The current state of the disclosure dialog if opened for metrics purposes. Histogram is only
    // recorded for active mode.
    private @Nullable Integer mDisclosureDialogState;

    // The current state of the mismatch dialog if opened for metrics purposes.
    private @Nullable Integer mMismatchDialogState;

    // Whether the user clicked the continue button for the mismatch dialog.
    // This button also dismisses the dialog, so we use this to prevent us from double counting the
    // Blink.FedCm.IdpSigninStatus.MismatchDialogResult metric
    private boolean mIsMismatchContinueClicked;

    // Whether there was a login to IDP CCT that was closed while the loading dialog is open. This
    // could mean that the user successfully completed the login to IDP flow.
    private boolean mIsLoadingDialogLoginToIdpClosed;

    // Whether this active mode sheet replaced the passive mode sheet of a passive mode request,
    // through `showRequestPermissionFromPassiveSheet()`.
    private boolean mSwitchedFromPassiveSheet;

    private final KeyboardVisibilityListener mKeyboardVisibilityListener =
            new KeyboardVisibilityListener() {
                @Override
                public void keyboardObscuringChanged(boolean isShowing) {
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
            ModalDialogManager modalDialogManager,
            boolean canShowUi,
            @Nullable ActiveSheetSwitcher activeSheetSwitcher) {
        assert tab != null;
        assert (rpMode == RpMode.PASSIVE) == (activeSheetSwitcher != null);
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
        mCanShowUi = canShowUi;
        mActiveSheetSwitcher = activeSheetSwitcher;
        if (mTab != null && mTab.getWebContents() != null) {
            mUkmRecorder = new UkmRecorder(mTab.getWebContents(), "Blink.FedCm");
        }

        mBottomSheetObserver =
                new BottomSheetObserver() {
                    // Sends focus events to the relevant views for accessibility.
                    // TODO(crbug.com/40262629): Add tests for TalkBack on FedCM.
                    private void focusForAccessibility() {
                        View contentView =
                                assumeNonNull(mBottomSheetController.getCurrentSheetContent())
                                        .getContentView();
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
                        // TODO(crbug.com/571856570): Remove mLastSheetSeen with the feature flag.
                        if (BottomSheetFeatureMap.sBottomSheetDeferContentSwapOnHidden.isEnabled()
                                ? mBottomSheetController.getCurrentSheetContent()
                                        != mBottomSheetContent
                                : mLastSheetSeen != mBottomSheetContent) {
                            return;
                        }
                        if (mWasDismissed) return;

                        if (state == SheetState.HIDDEN) {
                            // BottomSheetController.StateChangeReason.NONE happens for instance
                            // when the user opens the tab switcher or when the user leaves
                            // Chrome. We do not want to dismiss in those cases.
                            if (reason == BottomSheetController.StateChangeReason.NONE) {
                                mBottomSheetController.hideContent(mBottomSheetContent, true);
                            } else {
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
                                    mMismatchDialogState =
                                            mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC
                                                    ? MismatchDialogResult.SWIPE
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
                                    mMismatchDialogState =
                                            mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC
                                                    ? MismatchDialogResult.BACK_PRESS
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
                                    mMismatchDialogState =
                                            mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC
                                                    ? MismatchDialogResult.TAP_SCRIM
                                                    : null;
                                } else if (reason
                                        == BottomSheetController.StateChangeReason.CLOSE_BUTTON) {
                                    dismissReason = IdentityRequestDialogDismissReason.CLOSE_BUTTON;
                                    recordCloseSheetMetrics();
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
                    public void onSheetContentChanged(@Nullable BottomSheetContent bottomSheet) {
                        // Keep track of the latest sheet seen. Since this method is invoked before
                        // onSheetStateChanged() when the sheet is swiped out, we do not clear
                        // |mLastSheetSeen| if |bottomSheet| is null.
                        if (bottomSheet != null) {
                            mLastSheetSeen = bottomSheet;
                        }
                    }
                };

        mTabObserver =
                new TabObserver() {
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
                                                && assumeNonNull(mAccounts).size() != 1
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

    private PropertyModel createHeaderItem(String rpForDisplay, String iframeForDisplay) {
        Runnable closeOnClickRunnable = null;
        if (!mBottomSheetController.willShowFrameworkCloseButton(mBottomSheetContent)) {
            closeOnClickRunnable =
                    () -> {
                        onDismissed(IdentityRequestDialogDismissReason.CLOSE_BUTTON);
                        recordCloseSheetMetrics();
                    };
        }

        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.HEADER_ICON, mHeaderIcon)
                .with(HeaderProperties.RP_BRAND_ICON, mRpBrandIcon)
                .with(HeaderProperties.CLOSE_ON_CLICK_LISTENER, closeOnClickRunnable)
                .with(HeaderProperties.IDP_FOR_DISPLAY, mIdpForDisplay)
                .with(HeaderProperties.RP_FOR_DISPLAY, rpForDisplay)
                .with(HeaderProperties.IFRAME_FOR_DISPLAY, iframeForDisplay)
                .with(HeaderProperties.TYPE, mHeaderType)
                .with(HeaderProperties.RP_CONTEXT, mRpContext)
                .with(HeaderProperties.RP_MODE, mRpMode)
                .with(
                        HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER,
                        mSelectedAccount == null && mAccounts != null && mAccounts.size() > 1)
                .with(HeaderProperties.SET_FOCUS_VIEW_CALLBACK, this::setFocusView)
                .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, mIsShowingMultipleIdps)
                .with(HeaderProperties.IS_MULTI_IDP_REQUEST, mIsMultiIdpRequest)
                .build();
    }

    private int getSheetType() {
        switch (mHeaderType) {
            case HeaderType.SIGN_IN:
            case HeaderType.IDP_PICKER:
            case HeaderType.REQUEST_PERMISSION_MODAL:
                return SheetType.ACCOUNT_SELECTION;
            case HeaderType.VERIFY:
                return SheetType.VERIFYING;
            case HeaderType.VERIFY_AUTO_REAUTHN:
                return SheetType.AUTO_REAUTHN;
            case HeaderType.SIGN_IN_TO_IDP_STATIC:
                return SheetType.SIGN_IN_TO_IDP_STATIC;
            case HeaderType.SIGN_IN_ERROR:
                return SheetType.SIGN_IN_ERROR;
            case HeaderType.LOADING:
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

        if (mHeaderType == HeaderType.IDP_PICKER) {
            for (IdentityProviderData identityProvider : identityProviders) {
                mSheetAccountItems.add(
                        new ListItem(
                                AccountSelectionProperties.ITEM_TYPE_IDP,
                                createIdpItem(identityProvider)));
            }
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

    private void maybeRecordAccountChooserResult(int result) {
        if (mRpMode != RpMode.ACTIVE || mSwitchedFromPassiveSheet || mAccountChooserState == null) {
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.Button.AccountChooserResult",
                result,
                AccountChooserResult.MAX_VALUE + 1);
        if (mUkmRecorder != null) {
            mUkmRecorder.addMetric("Button.AccountChooserResult", result).record();
        }
        mAccountChooserState = null;
    }

    private void maybeRecordLoadingDialogResult() {
        // Passive mode has no loading dialog, also when the active mode sheet replaced the passive
        // mode one.
        if (mRpMode != RpMode.ACTIVE || mSwitchedFromPassiveSheet) return;

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
                "Blink.FedCm.Button.LoadingDialogResult",
                mLoadingDialogState,
                LoadingDialogResult.MAX_VALUE + 1);
        if (mUkmRecorder != null) {
            mUkmRecorder.addMetric("Button.LoadingDialogResult", mLoadingDialogState).record();
        }

        // Reset the state to prevent double-recording.
        mLoadingDialogState = null;
    }

    private void maybeRecordDisclosureDialogResult() {
        // Passive mode flows that switch to the active mode sheet are kept out of the `Button.*`
        // active mode metrics.
        // TODO(crbug.com/581372729): Record separate metrics for passive mode flows that switch to
        // the active mode sheet.
        if (mRpMode != RpMode.ACTIVE || mSwitchedFromPassiveSheet) return;

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
                DisclosureDialogResult.MAX_VALUE + 1);
        if (mUkmRecorder != null) {
            mUkmRecorder
                    .addMetric("Button.DisclosureDialogResult", mDisclosureDialogState)
                    .record();
        }

        // Reset the state to prevent double-recording.
        mDisclosureDialogState = null;
    }

    private void maybeRecordMismatchDialogResult() {
        // mMismatchDialogState is set on dismissal e.g. tap scrim, back press or if the user
        // presses continue. If it hasn't been set but onDismissed is called while the mismatch
        // dialog is being shown, then we don't know what caused the dismissal.
        if (!mIsMismatchContinueClicked
                && mMismatchDialogState == null
                && mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            mMismatchDialogState = MismatchDialogResult.DISMISSED_FOR_OTHER_REASONS;
        }

        if (mMismatchDialogState == null) return;

        assert mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC;

        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
                mMismatchDialogState,
                MismatchDialogResult.MAX_VALUE + 1);

        // Reset the state to prevent double-recording.
        mMismatchDialogState = null;
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
        if (TextUtils.isEmpty(mIdpForDisplay)) {
            // The previous sheet had no IDP to display because the request has multiple IDPs.
            mIdpForDisplay = account.getIdentityProviderData().getIdpForDisplay();
        }
        if (!updateSheet(
                Arrays.asList(account),
                /* identityProviders= */ Collections.emptyList(),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        updateBackPressBehavior();
        return true;
    }

    /**
     * Shows the request permission dialog for {@code account}, which the user selected in the
     * passive mode sheet that this active mode sheet replaces. See {@link
     * #shouldSwitchToActiveSheetForPermission(Account)}. The rest of the flow is handled like in
     * active mode, e.g. back shows the account chooser for {@code accounts}. Only called right
     * after this mediator is created.
     */
    void showRequestPermissionFromPassiveSheet(
            RelyingPartyData rpData,
            List<Account> accounts,
            List<IdentityProviderData> idpDataList,
            Account account) {
        assert mRpMode == RpMode.ACTIVE;
        assert mHeaderType == HeaderType.NONE;
        assert !mWasDismissed;
        mSwitchedFromPassiveSheet = true;
        mRpData = rpData;
        mAccounts = accounts;
        mIdpDataListForShowAccounts = idpDataList;
        // Like in active mode with multiple identity providers, the request permission dialog then
        // shows the RP icon of `account`'s identity provider, and back shows the account chooser
        // without login rows, see `updateSheet()`.
        mIsMultiIdpRequest = idpDataList.size() > 1;
        // Title the sheet with the identity provider of `account`.
        IdentityProviderData idpData = account.getIdentityProviderData();
        mIdpForDisplay = idpData.getIdpForDisplay();
        mRpContext = idpData.getRpContext();
        mSelectedAccount = account;
        // This sheet is first shown on the request permission dialog, so `showContent()` doesn't
        // notify the delegate that the accounts were displayed. The passive mode sheet already did.
        // `mSwitchedFromPassiveSheet` also keeps this sheet out of the `Button.*` active mode
        // metrics.
        // TODO(crbug.com/581372729): Record separate metrics for passive mode flows that switch to
        // the active mode sheet.
        if (!showRequestPermissionModalSheet(account)) {
            return;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
    }

    /** Whether this active mode sheet replaced the passive mode sheet of a passive mode request. */
    boolean wasSwitchedFromPassiveSheet() {
        return mSwitchedFromPassiveSheet;
    }

    // Dismisses content without notifying the delegate. Should only be invoked during destruction.
    void close() {
        if (!mWasDismissed) dismissContent(/* animate= */ true);
    }

    /**
     * Like {@link #close()}, but hides this sheet without animation. The coordinator uses it when
     * it replaces this sheet with a sheet for the other mode, so that the bottom sheet can show the
     * new sheet right away instead of queueing it behind this one.
     */
    void closeWithoutAnimation() {
        if (!mWasDismissed) dismissContent(/* animate= */ false);
    }

    boolean showAccounts(
            RelyingPartyData rpData,
            List<Account> accounts,
            List<IdentityProviderData> idpDataList,
            List<Account> newAccounts) {
        if (mWasDismissed) {
            return false;
        }
        mRpData = rpData;
        mAccounts = accounts;
        mIdpDataListForShowAccounts = idpDataList;
        mIdpMetadataForLoginOrError = null;
        setIsShowingMultipleIdps(mIdpDataListForShowAccounts.size() > 1);
        mIsMultiIdpRequest = mIdpDataListForShowAccounts.size() > 1;
        mRpContext = mIdpDataListForShowAccounts.get(0).getRpContext();
        mSelectedAccount = null;
        mLoadingDialogState =
                mIsLoadingDialogLoginToIdpClosed
                        ? LoadingDialogResult.PROCEED_THROUGH_POPUP
                        : LoadingDialogResult.PROCEED;
        maybeRecordLoadingDialogResult();

        // In passive mode, an account is automatically 'selected' if that is the only option.
        // Active mode first shows the single-account chooser without disclosure text, and
        // updateSheet() selects the account for its continue button.
        if (mRpMode == RpMode.PASSIVE
                && accounts.size() == 1
                && !mIsShowingMultipleIdps
                && !mIdpDataListForShowAccounts
                        .get(0)
                        .getIdpMetadata()
                        .showUseDifferentAccountButton()) {
            mSelectedAccount = accounts.get(0);
        }

        if (!showAccountsInternal(newAccounts)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        return true;
    }

    boolean showFailureDialog(
            RelyingPartyData rpData,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext) {
        mRpData = rpData;
        mIdpForDisplay = idpForDisplay;
        mIdpMetadataForLoginOrError = idpMetadata;
        mIdpDataListForShowAccounts = null;
        mRpContext = rpContext;
        mHeaderType = HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC;
        mIsMismatchContinueClicked = false;
        mMismatchDialogState = null;
        if (!updateSheet(
                /* accounts= */ Collections.emptyList(),
                /* identityProviders= */ Collections.emptyList(),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        updateHeader(
                idpMetadata.getBrandIconBitmap(),
                /* shouldCircleCrop= */ true,
                /* rpBrandIcon= */ null);
        return true;
    }

    boolean showErrorDialog(
            RelyingPartyData rpData,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext,
            IdentityCredentialTokenError error) {
        mRpData = rpData;
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
            updateHeader(
                    idpMetadata.getBrandIconBitmap(),
                    /* shouldCircleCrop= */ true,
                    /* rpBrandIcon= */ null);
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
        ErrorProperties.Properties properties =
                new ErrorProperties.Properties(
                        idpForDisplay,
                        mRpData.getRpForDisplay(),
                        error,
                        !error.getUrl().isEmpty() ? this::onMoreDetails : null);
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
            RelyingPartyData rpData, String idpForDisplay, @RpContext.EnumType int rpContext) {
        mRpData = rpData;
        mIdpForDisplay = idpForDisplay;
        // Content passes an empty IDP when the request has multiple IDPs.
        mIsMultiIdpRequest = idpForDisplay.isEmpty();
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

    boolean showVerifyingDialog(RelyingPartyData rpData, Account account, boolean isAutoReauthn) {
        mRpData = rpData;
        mHeaderType = isAutoReauthn ? HeaderType.VERIFY_AUTO_REAUTHN : HeaderType.VERIFY;
        mSelectedAccount = account;

        // Auto re-authn in active mode does not update the loading UI.
        if (mRpMode == RpMode.ACTIVE && isAutoReauthn) {
            return true;
        }

        showVerifySheet(mSelectedAccount);
        return true;
    }

    public void setCanShowUi(boolean canShowUi) {
        if (mCanShowUi == canShowUi) {
            return;
        }
        mCanShowUi = canShowUi;
        if (!mCanShowUi) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
        } else {
            showContent();
        }
    }

    /**
     * Whether FedCM UI may currently be shown. Flows that do not go through {@link #showContent()}
     * (such as the native app UI, which hands off to another activity) must consult this directly.
     */
    boolean canShowUi() {
        return mCanShowUi;
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
            case IdentityRequestDialogLinkType.IDP_LINK:
                // Not recorded, so that the histograms above only count the RP's links.
                // TODO(crbug.com/581372729): Record clicks on each of the identity provider's
                // links, and how often the links are shown.
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
    @HeaderType
    int getHeaderType() {
        return mHeaderType;
    }

    // Shows the account chooser if mSelectedAccount is null, or the sheet for the selected account
    // (the confirmation sheet in passive mode) otherwise. Callers returning to the chooser need to
    // clear mSelectedAccount first, as handleBackPress() does. Does not set the
    // AccountChooserResult placeholder, which stays in showAccountsInternal().
    private boolean showAccountChooser() {
        mHeaderType = HeaderType.SIGN_IN;
        // Show the selected account's IDP or only show the IDP if there is just one.
        if (mSelectedAccount != null) {
            mIdpForDisplay = mSelectedAccount.getIdentityProviderData().getIdpForDisplay();
        } else if (assumeNonNull(mIdpDataListForShowAccounts).size() == 1) {
            mIdpForDisplay = mIdpDataListForShowAccounts.get(0).getIdpForDisplay();
        } else {
            mIdpForDisplay = null;
        }

        // We want the accounts to be clickable if there is no preselected account or if it doesn't
        // need permission, which happens when `getFields()` is empty. Otherwise, the continue
        // button selects the preselected account, also when the sheet shows no disclosure text
        // because `shouldSwitchToActiveSheetForPermission()` is true.
        if (!updateSheet(
                mSelectedAccount != null
                        ? Arrays.asList(mSelectedAccount)
                        : assertNonNull(mAccounts),
                mSelectedAccount != null
                        ? Collections.emptyList()
                        : assertNonNull(mIdpDataListForShowAccounts),
                /* areAccountsClickable= */ mSelectedAccount == null
                        || mSelectedAccount.getFields().length == 0)) {
            return false;
        }
        updateBackPressBehavior();
        return true;
    }

    // TODO(crbug.com/581411351): Make private once the account chooser opens the IDP picker.
    @VisibleForTesting
    boolean showIdpPicker() {
        assert mRpMode == RpMode.ACTIVE;
        assert assumeNonNull(mIdpDataListForShowAccounts).size() > 1;
        mSelectedAccount = null;
        mHeaderType = HeaderType.IDP_PICKER;
        mIdpForDisplay = null;
        if (!updateSheet(
                Collections.emptyList(),
                assertNonNull(mIdpDataListForShowAccounts),
                /* areAccountsClickable= */ false)) {
            return false;
        }
        setComponentShowTime(SystemClock.elapsedRealtime());
        updateBackPressBehavior();
        return true;
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

            // We should show the permission dialog / disclosure text if fields
            // is not empty. This is affected by the login status as well as
            // the requested fields.
            boolean shouldShowRequestPermissionDialog = newlySignedInAccount.getFields().length > 0;
            if (shouldShowRequestPermissionDialog) {
                return showRequestPermissionModalSheet(mSelectedAccount);
            }

            // Else:
            // Show accounts picker which doesn't contain the disclosure text. We do not support
            // request permission UI without disclosure text.
        }

        if (!showAccountChooser()) {
            return false;
        }

        // This is a placeholder assuming the tab containing the account chooser will be closed.
        // This will be updated upon user action i.e. clicking on account row, use
        // other account button or swiped down. If we do not receive any of these actions by time
        // onDismissed() is called, it means our placeholder assumption is true i.e. the user has
        // closed the tab.
        if (mRpMode == RpMode.ACTIVE && !mSwitchedFromPassiveSheet) {
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
        Set<IdentityProviderData> distinctIdps = new HashSet<>(identityProviders);
        for (Account account : accounts) {
            distinctIdps.add(account.getIdentityProviderData());
        }
        // The loading sheet has no accounts or IDPs to count, so it uses the IDPs of the request.
        setIsShowingMultipleIdps(
                mHeaderType == HeaderType.LOADING ? mIsMultiIdpRequest : distinctIdps.size() > 1);
        IdentityProviderData uniqueIdp =
                distinctIdps.size() == 1 ? distinctIdps.iterator().next() : null;

        boolean isSingleAccountChooserActiveMode =
                isSingleAccountChooser
                        && !mIsShowingMultipleIdps
                        && mRpMode == RpMode.ACTIVE
                        && mHeaderType == HeaderType.SIGN_IN;
        boolean showUseDifferentAccountInSingleAccountChooserActiveMode =
                isSingleAccountChooserActiveMode
                        && areAccountsClickable
                        && firstAccountIdp != null
                        && firstAccountIdp.getIdpMetadata().showUseDifferentAccountButton();
        // We add the add account button alongside the accounts if supported in passive mode and in
        // the multi-account UI of active mode with a single IDP.
        // TODO(crbug.com/581411351): With multiple IDPs in active mode, offer the IDPs that have
        // no usable account in an IDP picker instead, as on desktop. When no IDP returns accounts,
        // AccountSelectionBridge.showAccounts() also asserts `!accounts.isEmpty()` and must
        // accept an empty list (crbug.com/586497366).
        boolean skipAddAccountRows =
                showUseDifferentAccountInSingleAccountChooserActiveMode
                        || (mRpMode == RpMode.ACTIVE && mIsMultiIdpRequest);
        updateAccounts(accounts, identityProviders, areAccountsClickable, skipAddAccountRows);
        // TODO(crbug.com/575550088): Show the RP icon in active mode with multiple IDPs. Until
        // then, the header shows a fallback icon, see AccountSelectionViewBinder.bindHeaderView().
        Bitmap headerIcon = null;
        if (uniqueIdp != null) {
            headerIcon = uniqueIdp.getIdpMetadata().getBrandIconBitmap();
        } else if (mIsShowingMultipleIdps && mRpMode == RpMode.PASSIVE) {
            headerIcon = TabFavicon.getBitmap(mTab);
        }
        // With multiple IDPs, the RP icon is the one the IDP of this sheet provides, e.g. the IDP
        // of the account the user picked.
        IdentityProviderData rpBrandIconIdp = uniqueIdp;
        if (!mIsMultiIdpRequest) {
            rpBrandIconIdp =
                    mIdpDataListForShowAccounts != null ? mIdpDataListForShowAccounts.get(0) : null;
        }
        Bitmap rpBrandIcon =
                mRpMode == RpMode.ACTIVE && rpBrandIconIdp != null
                        ? rpBrandIconIdp.getClientMetadata().getBrandIconBitmap()
                        : null;
        // If there is a change in the header, setFocusView() will be called and focus will land on
        // the header when screen reader is on. Since the header is updated before any item is
        // created, the header will always take precedence for focus. Do not reorder this
        // updateHeader() call to happen after item creation.
        updateHeader(headerIcon, /* shouldCircleCrop= */ uniqueIdp != null, rpBrandIcon);

        boolean isDataSharingConsentVisible = false;
        Callback<ButtonData> continueButtonCallback = null;
        // The account that the continue button passes to `continueButtonCallback`, if any.
        Account continueButtonAccount = mSelectedAccount;
        if (mHeaderType == HeaderType.SIGN_IN && mSelectedAccount != null) {
            // An account that switches to the active mode sheet for permission gets its disclosure
            // text there instead.
            isDataSharingConsentVisible =
                    mSelectedAccount.getFields().length > 0
                            && !shouldSwitchToActiveSheetForPermission(mSelectedAccount);
            continueButtonCallback = this::onClickAccountSelected;
        }

        if (mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN) {
            assert mSelectedAccount != null;
            assert mSelectedAccount.isIdpClaimedSignIn()
                    || mSelectedAccount.isBrowserTrustedSignIn();
        }

        if (mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            assert !isDataSharingConsentVisible;
            assert mSelectedAccount == null;
            continueButtonCallback = this::onLoginToIdP;
        }

        if (isSingleAccountChooserActiveMode) {
            // The active mode single-account chooser shows no disclosure text, and its continue
            // button selects the only account. This doesn't set `mSelectedAccount`;
            // onAccountSelected() does once the user selects the account.
            assert !isDataSharingConsentVisible;
            continueButtonAccount = accounts.get(0);
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
                            ? createDataSharingConsentItem(assumeNonNull(mSelectedAccount))
                            : null);
        }
        mModel.set(
                ItemProperties.CONTINUE_BUTTON,
                (continueButtonCallback != null)
                        ? createContinueBtnItem(
                                continueButtonAccount,
                                continueButtonAccount != null
                                        ? continueButtonAccount
                                                .getIdentityProviderData()
                                                .getIdpMetadata()
                                        : assertNonNull(mIdpMetadataForLoginOrError),
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
                            ? createDataSharingConsentItem(assumeNonNull(mSelectedAccount))
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
                        ? createErrorTextItem(mIdpForDisplay, assertNonNull(mError))
                        : null);
        // The add account button is added separately for active mode single account chooser.
        mModel.set(
                ItemProperties.ADD_ACCOUNT_BUTTON,
                showUseDifferentAccountInSingleAccountChooserActiveMode
                        ? createLoginBtnItem(assertNonNull(firstAccountIdp))
                        : null);
        mModel.set(
                ItemProperties.ACCOUNT_CHIP,
                mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                        ? createAccountItem(
                                assertNonNull(mSelectedAccount), /* isAccountClickable= */ false)
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
                ItemProperties.DRAGBAR_HANDLE_VISIBLE,
                mRpMode == RpMode.ACTIVE || mIsShowingMultipleIdps);

        mBottomSheetController.expandSheet();
        // When a user opens a page that invokes the FedCM API in a new tab, the tab will be hidden
        // and we should not show the bottom sheet to avoid confusion.
        mTab.addObserver(mTabObserver);
        if (!mTab.isHidden()) {
            return showContent();
        }
        return true;
    }

    private void updateHeader(
            @Nullable Bitmap headerIcon, boolean shouldCircleCrop, @Nullable Bitmap rpBrandIcon) {
        mHeaderIcon =
                AccountSelectionViewBinder.isValidBrandIcon(headerIcon, mRpMode, shouldCircleCrop)
                        ? headerIcon
                        : null;
        mRpBrandIcon =
                AccountSelectionViewBinder.isValidBrandIcon(
                                rpBrandIcon, mRpMode, /* shouldCircleCrop= */ true)
                        ? rpBrandIcon
                        : null;
        String rpForDisplay = "";
        String iframeForDisplay = "";
        assumeNonNull(mRpData);
        if (!mRpData.getDisplayStringsMayChange()) {
            rpForDisplay = mRpData.getRpForDisplay();
            iframeForDisplay = mRpData.getIframeForDisplay();
        }
        PropertyModel headerModel = createHeaderItem(rpForDisplay, iframeForDisplay);
        mModel.set(ItemProperties.HEADER, headerModel);
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    private boolean showContent() {
        if (mWasDismissed || mIsModalDialogOpen || !mCanShowUi) {
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
                || mHeaderType == HeaderType.IDP_PICKER
                || mHeaderType == HeaderType.VERIFY
                || mHeaderType == HeaderType.VERIFY_AUTO_REAUTHN) {
            mDelegate.onAccountsDisplayed();
        }
        mBottomSheetController.addObserver(mBottomSheetObserver);
        KeyboardVisibilityDelegate.getInstance()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mTab.addObserver(mTabObserver);
        return true;
    }

    /** Requests to dismiss bottomsheet. */
    private void dismissContent(boolean animate) {
        mWasDismissed = true;
        KeyboardVisibilityDelegate.getInstance()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mTab.removeObserver(mTabObserver);
        mBottomSheetController.hideContent(mBottomSheetContent, animate);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        updateBackPressBehavior();
    }

    private void requestAvatarImage(PropertyModel accountModel) {
        Account account = accountModel.get(AccountProperties.ACCOUNT);
        final String displayName = account.getDisplayName();
        final Bitmap picture =
                mIsShowingMultipleIdps
                        ? account.getCircledBadgedPictureBitmap()
                        : account.getPictureBitmap();
        int avatarSize =
                mIsShowingMultipleIdps
                        ? mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.account_selection_account_avatar_multi_idp_size)
                        : mDesiredAvatarSize;
        accountModel.set(
                AccountProperties.AVATAR,
                new AccountProperties.Avatar(displayName, picture, avatarSize));
    }

    private void setIsShowingMultipleIdps(boolean isShowingMultipleIdps) {
        mIsShowingMultipleIdps = isShowingMultipleIdps;
        mBottomSheetContent.setIsShowingMultipleIdps(isShowingMultipleIdps);
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
        if (mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            mIsMismatchContinueClicked = true;
            mMismatchDialogState = MismatchDialogResult.CONTINUED;
            maybeRecordMismatchDialogResult();
        }
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
        // FedCM. It can also come from the passive mode single-account chooser, which shows no
        // disclosure text when `shouldSwitchToActiveSheetForPermission()` is true.
        Account oldSelectedAccount = mSelectedAccount;
        mSelectedAccount = buttonData.mAccount;
        boolean switchToActiveSheet = shouldSwitchToActiveSheetForPermission(mSelectedAccount);

        // If the account is selected from UI which shows the disclosure text or
        // if the browser doesn't need to request permission, skip the
        // disclosure UI and proceed to the verifying sheet.
        if ((mRpMode == RpMode.PASSIVE && oldSelectedAccount != null && !switchToActiveSheet)
                || mHeaderType == HeaderType.REQUEST_PERMISSION_MODAL
                || mSelectedAccount.getFields().length == 0) {
            mDelegate.onAccountSelected(mSelectedAccount);
            showVerifySheet(mSelectedAccount);
            return;
        }

        // At this point, the account is a non-returning user. If
        // `shouldSwitchToActiveSheetForPermission()` is true, replace the passive mode sheet with
        // the active mode sheet's request permission dialog.
        if (switchToActiveSheet) {
            // The flow continues in the active mode sheet, so don't notify the delegate.
            assumeNonNull(mActiveSheetSwitcher)
                    .switchToActiveSheetForPermission(
                            assumeNonNull(mRpData),
                            assumeNonNull(mAccounts),
                            assumeNonNull(mIdpDataListForShowAccounts),
                            mSelectedAccount);
            return;
        }

        // In active mode, show the request permission dialog.
        if (mRpMode == RpMode.ACTIVE) {
            showRequestPermissionModalSheet(mSelectedAccount);
            return;
        }

        // At this point, the account is a non-returning user and RP mode is widget.
        showAccountsInternal(/* newAccounts= */ null);
    }

    /**
     * Returns whether selecting {@code account} in the passive mode sheet replaces it with the
     * active mode sheet's request permission dialog, which also shows the identity provider's
     * links, instead of asking for permission in the passive mode sheet. If so, the passive mode
     * sheet doesn't show disclosure text for {@code account}. True only with `FedCmIdpLinks` and an
     * account that needs permission to share its fields. With multiple identity providers, it also
     * requires `FedCmActiveModeMultipleIdentityProviders`.
     */
    private boolean shouldSwitchToActiveSheetForPermission(Account account) {
        // Like on desktop, require `FedCmActiveModeMultipleIdentityProviders` with multiple
        // identity providers, since back on the active mode sheet then shows its account chooser
        // for multiple identity providers, which is otherwise only shown in active mode with that
        // flag.
        return ChromeFeatureList.sFedCmIdpLinks.isEnabled()
                && mRpMode == RpMode.PASSIVE
                && account.getFields().length > 0
                && (assertNonNull(mIdpDataListForShowAccounts).size() == 1
                        || ContentFeatureMap.isEnabled(
                                ContentFeatures.FED_CM_ACTIVE_MODE_MULTIPLE_IDENTITY_PROVIDERS));
    }

    private void recordCloseSheetMetrics() {
        RecordHistogram.recordEnumeratedHistogram(
                "Blink.FedCm.ClosedSheetType.Android", getSheetType(), SheetType.MAX_VALUE + 1);
    }

    void onDismissed(@IdentityRequestDialogDismissReason int dismissReason) {
        if (mWasDismissed) {
            return;
        }

        // If dismissed from the close button for mismatch dialog, set the dialog state for
        // recording the dialog result.
        if (dismissReason == IdentityRequestDialogDismissReason.CLOSE_BUTTON
                && mHeaderType == HeaderType.SIGN_IN_TO_IDP_STATIC) {
            mMismatchDialogState = MismatchDialogResult.DISMISSED_BY_CLOSE_ICON;
        }

        if (mAccountChooserState != null) {
            maybeRecordAccountChooserResult(mAccountChooserState);
        }
        maybeRecordLoadingDialogResult();
        maybeRecordDisclosureDialogResult();
        maybeRecordMismatchDialogResult();
        dismissContent(/* animate= */ true);
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
                        .with(AccountProperties.SHOW_IDP, mIsShowingMultipleIdps)
                        .build();
        requestAvatarImage(model);
        return model;
    }

    private PropertyModel createContinueBtnItem(
            @Nullable Account account,
            IdentityProviderMetadata idpMetadata,
            Callback<ButtonData> onClickListener) {
        assert account != null
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN_ERROR
                || mHeaderType == HeaderProperties.HeaderType.SIGN_IN;

        ContinueButtonProperties.Properties properties =
                new ContinueButtonProperties.Properties(
                        account, idpMetadata, onClickListener, mHeaderType, this::setFocusView);
        return new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                .with(ContinueButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createLoginBtnItem(IdentityProviderData identityProvider) {
        LoginButtonProperties.Properties properties =
                new LoginButtonProperties.Properties(
                        identityProvider, this::onLoginToIdP, mRpMode, mIsShowingMultipleIdps);
        return new PropertyModel.Builder(LoginButtonProperties.ALL_KEYS)
                .with(LoginButtonProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createIdpItem(IdentityProviderData identityProvider) {
        IdpProperties.Properties properties =
                new IdpProperties.Properties(identityProvider, this::onLoginToIdP);
        return new PropertyModel.Builder(IdpProperties.ALL_KEYS)
                .with(IdpProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createDataSharingConsentItem(Account account) {
        IdentityProviderData identityProvider = account.getIdentityProviderData();
        ClientIdMetadata metadata = identityProvider.getClientMetadata();
        IdentityProviderMetadata idpMetadata = identityProvider.getIdpMetadata();
        // Only the active mode sheet shows the identity provider's links, in the container UI.
        boolean showIdpLinks =
                mRpMode == RpMode.ACTIVE && ChromeFeatureList.sFedCmIdpLinks.isEnabled();
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties(
                        identityProvider.getIdpForDisplay(),
                        metadata.getTermsOfServiceUrl(),
                        metadata.getPrivacyPolicyUrl(),
                        (Context context) -> {
                            showUrl(
                                    context,
                                    IdentityRequestDialogLinkType.TERMS_OF_SERVICE,
                                    metadata.getTermsOfServiceUrl());
                        },
                        (Context context) -> {
                            showUrl(
                                    context,
                                    IdentityRequestDialogLinkType.PRIVACY_POLICY,
                                    metadata.getPrivacyPolicyUrl());
                        },
                        showIdpLinks ? idpMetadata.getAccountManagementUrl() : GURL.emptyGURL(),
                        showIdpLinks ? idpMetadata.getPrivacyPolicyUrl() : GURL.emptyGURL(),
                        showIdpLinks ? idpMetadata.getTermsOfServiceUrl() : GURL.emptyGURL(),
                        (Context context, GURL url) -> {
                            showUrl(context, IdentityRequestDialogLinkType.IDP_LINK, url);
                        },
                        this::setFocusView,
                        account.getFields());

        return new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                .with(DataSharingConsentProperties.PROPERTIES, properties)
                .build();
    }

    private PropertyModel createIdpSignInItem(@Nullable String idpForDisplay) {
        return new PropertyModel.Builder(IdpSignInProperties.ALL_KEYS)
                .with(IdpSignInProperties.IDP_FOR_DISPLAY, idpForDisplay)
                .build();
    }

    private PropertyModel createErrorTextItem(
            @Nullable String idpForDisplay, IdentityCredentialTokenError error) {
        ErrorProperties.Properties properties =
                new ErrorProperties.Properties(
                        idpForDisplay,
                        assumeNonNull(mRpData).getRpForDisplay(),
                        error,
                        !error.getUrl().isEmpty() ? this::onMoreDetails : null);
        return new PropertyModel.Builder(ErrorProperties.ALL_KEYS)
                .with(ErrorProperties.PROPERTIES, properties)
                .build();
    }

    void onModalDialogOpened() {
        mIsModalDialogOpen = true;
    }

    // Called when the login modal finishes and is closed programmatically, as opposed to
    // `onModalDialogClosedByUser()` when the user dismisses it before completion.
    void onModalDialogClosed() {
        mIsModalDialogOpen = false;
        if (!mIsLoadingDialogLoginToIdpClosed && mHeaderType == HeaderType.LOADING) {
            mIsLoadingDialogLoginToIdpClosed = true;
        }
    }

    // Handles the user closing the login CCT or native app intent before completion. Re-shows the
    // account chooser when opened from `SIGN_IN`; otherwise dismisses the request because there is
    // no chooser to return to (matching desktop's `user_close_cancels_flow`).
    void onModalDialogClosedByUser() {
        if (mWasDismissed) {
            return;
        }
        boolean wasModalDialogOpen = mIsModalDialogOpen;
        mIsModalDialogOpen = false;
        if (mHeaderType == HeaderType.SIGN_IN && wasModalDialogOpen) {
            mSelectedAccount = null;
            showAccountsInternal(/* newAccounts= */ null);
            return;
        }
        onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }
}

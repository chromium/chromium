// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CANCEL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.ERROR_STATE_INFO;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LOADING_INDICATOR_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_LOADING_VIEW_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PROPOSED_PLUS_ADDRESS;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.SHOW_ONBOARDING_NOTICE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Mediator class for the plus address creation UI.
 *
 * <p>This component shows a bottom sheet to let the user create a plus address.
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiShown, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class PlusAddressCreationMediator extends EmptyBottomSheetObserver
        implements PlusAddressCreationDelegate, TabModelObserver, LayoutStateObserver {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModelSelector mTabModelSelector;
    private final TabModel mTabModel;
    private final PlusAddressCreationViewBridge mBridge;
    private PropertyModel mModel;
    @Nullable private String mProposedPlusAddress;
    @Nullable private PlusAddressCreationErrorStateInfo mErrorStateInfo;

    /**
     * Creates the mediator.
     *
     * @param context Current application context.
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     * @param tabModelSelector The TabModelSelector for tabModel
     * @param bridge The bridge to signal UI flow events (onConfirmed, onCanceled, etc.) to.
     */
    PlusAddressCreationMediator(
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            PlusAddressCreationViewBridge bridge) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;
        mTabModelSelector = tabModelSelector;
        mBridge = bridge;

        mBottomSheetController.addObserver(this);
        mLayoutStateProvider.addObserver(this);
        mTabModel.addObserver(this);
    }

    void setModel(PropertyModel model) {
        mModel = model;
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        mModel.set(VISIBLE, true);
    }

    void updateProposedPlusAddress(String plusAddress) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)) {
            mProposedPlusAddress = plusAddress;
            mModel.set(PLUS_ADDRESS_LOADING_VIEW_VISIBLE, false);
        } else {
            mModel.set(PROPOSED_PLUS_ADDRESS, plusAddress);
            mModel.set(REFRESH_ICON_ENABLED, true);
            mModel.set(CONFIRM_BUTTON_ENABLED, true);
        }
    }

    @Override
    public void onPlusAddressLoadingViewHidden() {
        // Loading view gets hidden during the initial property binding if the feature is disabled.
        // Proposed plus address should not be updated in this case.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)) {
            mModel.set(PLUS_ADDRESS_ICON_VISIBLE, true);
            mModel.set(PROPOSED_PLUS_ADDRESS, mProposedPlusAddress);
            mModel.set(REFRESH_ICON_ENABLED, true);
            mModel.set(CONFIRM_BUTTON_ENABLED, true);
        }
    }

    void showError(@Nullable PlusAddressCreationErrorStateInfo errorStateInfo) {
        if (errorStateInfo == null) {
            mModel.set(CONFIRM_BUTTON_ENABLED, false);
            mModel.set(CONFIRM_BUTTON_VISIBLE, true);
            if (mModel.get(SHOW_ONBOARDING_NOTICE)) {
                mModel.set(CANCEL_BUTTON_VISIBLE, true);
            }
            mModel.set(LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE, true);
            mModel.set(LOADING_INDICATOR_VISIBLE, false);
            return;
        }
        if (mModel.get(LOADING_INDICATOR_VISIBLE)) {
            // If the loading view is visible, hide it first and then show the error screen to avoid
            // UI glitches.
            mErrorStateInfo = errorStateInfo;
            mModel.set(LOADING_INDICATOR_VISIBLE, false);
        } else {
            mModel.set(ERROR_STATE_INFO, errorStateInfo);
        }
    }

    void hideRefreshButton() {
        mModel.set(REFRESH_ICON_VISIBLE, false);
    }

    /** Hide the bottom sheet (if showing) and clean up observers. */
    void destroy() {
        mModel.set(VISIBLE, false);
        mBottomSheetController.removeObserver(this);
        mLayoutStateProvider.removeObserver(this);
        mTabModel.removeObserver(this);
    }

    // PlusAddressCreationDelegate implementation:
    @Override
    public void onRefreshClicked() {
        mModel.set(
                PROPOSED_PLUS_ADDRESS,
                mContext.getString(
                        R.string
                                .plus_address_model_refresh_temporary_label_content_android_to_migrate));
        mModel.set(REFRESH_ICON_ENABLED, false);
        mModel.set(CONFIRM_BUTTON_ENABLED, false);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED)) {
            mModel.set(PLUS_ADDRESS_ICON_VISIBLE, false);
            mModel.set(PLUS_ADDRESS_LOADING_VIEW_VISIBLE, true);
        }
        mBridge.onRefreshClicked();
    }

    @Override
    public void onConfirmRequested() {
        mModel.set(REFRESH_ICON_ENABLED, false);
        mModel.set(CONFIRM_BUTTON_ENABLED, false);
        mModel.set(CONFIRM_BUTTON_VISIBLE, false);
        mModel.set(
                CANCEL_BUTTON_VISIBLE,
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED));
        mModel.set(LOADING_INDICATOR_VISIBLE, true);
        mBridge.onConfirmRequested();
    }

    @Override
    public void onConfirmationLoadingViewHidden() {
        if (mModel.get(VISIBLE) && mErrorStateInfo != null) {
            mModel.set(ERROR_STATE_INFO, mErrorStateInfo);
            mErrorStateInfo = null;
        }
    }

    @Override
    public void onTryAgain() {
        boolean wasPlusAddressReserved = mModel.get(ERROR_STATE_INFO).wasPlusAddressReserved();
        mModel.set(ERROR_STATE_INFO, null);
        if (wasPlusAddressReserved) {
            onConfirmRequested();
        } else {
            mBridge.tryAgainToReservePlusAddress();
        }
    }

    @Override
    public void onCanceled() {
        mModel.set(VISIBLE, false);
        mBridge.onCanceled();
    }

    @Override
    public void onConfirmFinished() {
        mModel.set(VISIBLE, false);
    }

    @Override
    public void onPromptDismissed() {
        mModel.set(VISIBLE, false);
        mBridge.onPromptDismissed();
    }

    @Override
    public void openUrl(GURL url) {
        mTabModelSelector.openNewTab(
                new LoadUrlParams(url.getSpec()),
                TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(),
                /* incognito= */ false);
    }

    // EmptyBottomSheetObserver overridden methods follow:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        mModel.set(VISIBLE, false);
        // Swipe to dismiss should record cancel metrics.
        if (reason == StateChangeReason.SWIPE) {
            mBridge.onCanceled();
        }
        this.onPromptDismissed();
    }

    // TabModelObserver
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        // While the bottom sheet scrim covers the omnibox UI, a new tab can be created in other
        // ways such as by opening a link from another app. In this case we want to hide the bottom
        // sheet rather than keeping the bottom sheet open while this tab loads behind the scrim.
        if (lastId != tab.getId()) {
            mModel.set(VISIBLE, false);
        }
    }

    // LayoutStateObserver
    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        // When the browser layout changes away from browsing to say the tab switcher, then the
        // bottom sheet must be hidden.
        if (layoutType != LayoutType.BROWSING) {
            mModel.set(VISIBLE, false);
        }
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CANCEL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.DELEGATE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LOADING_INDICATOR_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.NORMAL_STATE_INFO;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the plus address creation UI. */
public class PlusAddressCreationCoordinator {
    private PlusAddressCreationMediator mMediator;

    public PlusAddressCreationCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            PlusAddressCreationViewBridge bridge,
            PlusAddressCreationNormalStateInfo info,
            boolean refreshSupported) {
        mMediator =
                new PlusAddressCreationMediator(
                        context,
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel,
                        tabModelSelector,
                        bridge);
        PropertyModel model = createDefaultModel(info, mMediator, refreshSupported);
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(context, bottomSheetController);

        mMediator.setModel(model);

        PropertyModelChangeProcessor.create(
                model,
                bottomSheetContent,
                PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void updateProposedPlusAddress(String plusAddress) {
        mMediator.updateProposedPlusAddress(plusAddress);
    }

    public void showError(@Nullable PlusAddressCreationErrorStateInfo errorStateInfo) {
        mMediator.showError(errorStateInfo);
    }

    public void hideRefreshButton() {
        mMediator.hideRefreshButton();
    }

    public void finishConfirm() {
        mMediator.onConfirmFinished();
    }

    public void destroy() {
        mMediator.destroy();
    }

    public void setMediatorForTesting(PlusAddressCreationMediator mediator) {
        mMediator = mediator;
    }

    static PropertyModel createDefaultModel(
            PlusAddressCreationNormalStateInfo normalStateInfo,
            PlusAddressCreationDelegate delegate,
            boolean refreshSupported) {
        final boolean showOnboardingNotice = !normalStateInfo.getNotice().isEmpty();
        return new PropertyModel.Builder(ALL_KEYS)
                .with(NORMAL_STATE_INFO, normalStateInfo)
                .with(DELEGATE, delegate)
                .with(SHOW_ONBOARDING_NOTICE, showOnboardingNotice)
                .with(VISIBLE, false)
                .with(
                        PLUS_ADDRESS_ICON_VISIBLE,
                        !ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED))
                .with(
                        PLUS_ADDRESS_LOADING_VIEW_VISIBLE,
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED))
                .with(PROPOSED_PLUS_ADDRESS, normalStateInfo.getProposedPlusAddressPlaceholder())
                .with(REFRESH_ICON_ENABLED, false)
                .with(REFRESH_ICON_VISIBLE, refreshSupported)
                .with(CONFIRM_BUTTON_ENABLED, false)
                .with(CONFIRM_BUTTON_VISIBLE, true)
                .with(CANCEL_BUTTON_VISIBLE, showOnboardingNotice)
                .with(LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE, false)
                .with(LOADING_INDICATOR_VISIBLE, false)
                .build();
    }
}

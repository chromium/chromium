// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Helper class that encapsulates the logic for which optional button is displayed on the browsing
 * mode toolbar.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class OptionalBrowsingModeButtonController {
    private final UserEducationHelper mUserEducationHelper;
    private final Map<ButtonDataProvider, ButtonDataProvider.ButtonDataObserver> mObserverMap;
    private ButtonDataProvider mCurrentProvider;
    private final ToolbarLayout mToolbarLayout;
    private final Supplier<Tab> mTabSupplier;

    /**
     * Creates a new OptionalBrowsingModeButtonController.
     * @param userEducationHelper Helper for displaying in-product help on a button.
     * @param toolbarLayout Toolbar layout where buttons will be displayed.
     */
    OptionalBrowsingModeButtonController(UserEducationHelper userEducationHelper,
                                         ToolbarLayout toolbarLayout,
            Supplier<Tab> tabSupplier) {
        mUserEducationHelper = userEducationHelper;
        mToolbarLayout = toolbarLayout;
        mTabSupplier = tabSupplier;
        mObserverMap = new HashMap<>();
    }

    public void destroy() {
        for (Map.Entry<ButtonDataProvider, ButtonDataProvider.ButtonDataObserver> entry :
                mObserverMap.entrySet()) {
            entry.getKey().removeObserver(entry.getValue());
        }

        mObserverMap.clear();
    }

    void updateButtonVisibility() {
        showHighestPrecedenceOptionalButton();
    }

    @VisibleForTesting
    void buttonDataProviderChanged(ButtonDataProvider provider, boolean canShowHint) {
        // If the updated button is showing, update it.
        if (provider == mCurrentProvider) {
            updateCurrentOptionalButton(provider);
            return;
        }

        // If the updated button isn't showing but could be, re-evaluate the highest precedence
        // button and show it.
        if (canShowHint) {
            showHighestPrecedenceOptionalButton();
        }

        // Otherwise, the updated button wasn't showing before and can't show now, so there's
        // nothing to do.
    }

    /**
     * Show the highest precedence optional button, hiding the optional button if none can be shown.
     */
    private void showHighestPrecedenceOptionalButton() {

    }

    /**
     * Sets the current optional button to reflect the given provider and button data, either
     * showing the button or updating it in place.
     */
    private void setCurrentOptionalButton(ButtonDataProvider provider, ButtonData buttonData) {
        mCurrentProvider = provider;
        mToolbarLayout.updateOptionalButton(buttonData);
        if (buttonData.getButtonSpec().getIPHCommandBuilder() != null) {
            mUserEducationHelper.requestShowIPH(
                    buttonData.getButtonSpec()
                            .getIPHCommandBuilder()
                            .setAnchorView(mToolbarLayout.getOptionalButtonView())
                            .build());
        }
    }

    private void hideCurrentOptionalButton() {
        mToolbarLayout.hideOptionalButton();
        mCurrentProvider = null;
    }

    /**
     * Processes an update to the current optional button, either mutating the view to reflect
     * ButtonData changes or hiding it entirely if it can no longer show.
     */
    private void updateCurrentOptionalButton(ButtonDataProvider provider) {
        ButtonData buttonData = provider.get(mTabSupplier.get());
        if (buttonData != null && buttonData.canShow()) {
            setCurrentOptionalButton(provider, buttonData);
        } else {
            hideCurrentOptionalButton();
            showHighestPrecedenceOptionalButton();
        }
    }

}

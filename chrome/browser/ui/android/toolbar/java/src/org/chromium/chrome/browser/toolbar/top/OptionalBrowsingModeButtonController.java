// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
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
    private List<ButtonDataProvider> mButtonDataProviders;
    private final ToolbarLayout mToolbarLayout;
    private final Supplier<Tab> mTabSupplier;

    /**
     * Creates a new OptionalBrowsingModeButtonController.
     * @param buttonDataProviders List of button data providers in precedence order.
     * @param userEducationHelper Helper for displaying in-product help on a button.
     * @param toolbarLayout Toolbar layout where buttons will be displayed.
     */
    OptionalBrowsingModeButtonController(
            List<ButtonDataProvider> buttonDataProviders,
            UserEducationHelper userEducationHelper,
            ToolbarLayout toolbarLayout,
            Supplier<Tab> tabSupplier) {
        mButtonDataProviders = buttonDataProviders;
        mUserEducationHelper = userEducationHelper;
        mToolbarLayout = toolbarLayout;
        mTabSupplier = tabSupplier;
        mObserverMap = new HashMap<>(buttonDataProviders.size());
        for (ButtonDataProvider provider : buttonDataProviders) {
            ButtonDataProvider.ButtonDataObserver callback =
                    (hint) -> buttonDataProviderChanged(provider, hint);
            provider.addObserver(callback);
            mObserverMap.put(provider, callback);
        }
    }

    public void destroy() {
        for (Map.Entry<ButtonDataProvider, ButtonDataProvider.ButtonDataObserver> entry :
                mObserverMap.entrySet()) {
            entry.getKey().removeObserver(entry.getValue());
        }

        mObserverMap.clear();
    }

    /**
     * Gets the {@link AdaptiveToolbarButtonVariant} of the currently shown button. {@code
     * AdaptiveToolbarButtonVariant.NONE} is returned if there's no visible button.
     * @return A value from {@link AdaptiveToolbarButtonVariant}.
     */
    public @AdaptiveToolbarButtonVariant int getCurrentButtonVariant() {
        if (mCurrentProvider == null || mTabSupplier == null) {
            return AdaptiveToolbarButtonVariant.NONE;
        }

        ButtonData currentButton = mCurrentProvider.get(mTabSupplier.get());

        if (currentButton == null || !currentButton.canShow()) {
            return AdaptiveToolbarButtonVariant.NONE;
        }

        return currentButton.getButtonSpec().getButtonVariant();
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
        if (mButtonDataProviders == null) return;
        for (ButtonDataProvider provider : mButtonDataProviders) {
            ButtonData buttonData = provider.get(mTabSupplier.get());
            if (buttonData != null && buttonData.canShow()) {
                // Same-provider updates are handled in updateCurrentOptionalButton; the below check
                // prevents us from pointlessly updating with the same button data.
                if (provider == mCurrentProvider) return;
                setCurrentOptionalButton(provider, buttonData);
                return;
            }
        }

        // If no buttons can show, hide the currently showing button.
        hideCurrentOptionalButton();
    }

    /**
     * Sets the current optional button to reflect the given provider and button data, either
     * showing the button or updating it in place.
     */
    private void setCurrentOptionalButton(ButtonDataProvider provider, ButtonData buttonData) {
        mCurrentProvider = provider;
        mToolbarLayout.updateOptionalButton(buttonData);
        // ToolbarPhone's optional button has animated transitions and it takes care of showing IPH
        // on its own.
        if (buttonData.getButtonSpec().getIPHCommandBuilder() != null
                && !(mToolbarLayout instanceof ToolbarPhone)) {
            mUserEducationHelper.requestShowIPH(
                    buttonData.getButtonSpec().getIPHCommandBuilder().build());
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

    /** Returns the list of {@link ButtonDataProvider}s. */
    public List<ButtonDataProvider> getButtonDataProvidersForTesting() {
        return mButtonDataProviders;
    }
}

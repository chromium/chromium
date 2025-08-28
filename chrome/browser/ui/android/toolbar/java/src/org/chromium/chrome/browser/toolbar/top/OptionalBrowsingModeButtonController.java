// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/**
 * Helper class that encapsulates the logic for which optional button is displayed on the browsing
 * mode toolbar.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class OptionalBrowsingModeButtonController {

    /** Delegate for handling the optional button on the toolbar. */
    public interface Delegate {
        /**
         * Sets the optional button data.
         *
         * @param buttonData {@link ButtonData} needed to show the optional button. The button will
         *     be hidden if {@code buttonData} is {@code null} or if there isn't enough space within
         *     the toolbar.
         */
        void setOptionalButtonData(@Nullable ButtonData buttonData);

        /** Whether the optional button is visible. */
        boolean isOptionalButtonVisible();
    }

    private final UserEducationHelper mUserEducationHelper;
    private final Map<ButtonDataProvider, ButtonDataProvider.ButtonDataObserver> mObserverMap;
    private @Nullable ButtonDataProvider mCurrentProvider;
    private final List<ButtonDataProvider> mButtonDataProviders;
    private final ToolbarLayout mToolbarLayout;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private OptionalBrowsingModeButtonController.@Nullable Delegate mDelegate;

    /**
     * Creates a new OptionalBrowsingModeButtonController.
     *
     * @param buttonDataProviders List of button data providers in precedence order.
     * @param userEducationHelper Helper for displaying in-product help on a button.
     * @param toolbarLayout Toolbar layout where buttons will be displayed.
     */
    OptionalBrowsingModeButtonController(
            List<ButtonDataProvider> buttonDataProviders,
            UserEducationHelper userEducationHelper,
            ToolbarLayout toolbarLayout,
            Supplier<@Nullable Tab> tabSupplier) {
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

    /**
     * Sets the delegate for the optional button. Once set, the delegate will be used to show or
     * hide the optional button on the toolbar based on the button data.
     *
     * @param delegate The {@link Delegate}.
     */
    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
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
        if (mDelegate != null) {
            mDelegate.setOptionalButtonData(buttonData);
        } else {
            mToolbarLayout.updateOptionalButton(buttonData);
        }
        // ToolbarPhone's optional button has animated transitions and it takes care of showing IPH
        // on its own.
        if (buttonData.getButtonSpec().getIphCommandBuilder() != null
                && !(mToolbarLayout instanceof ToolbarPhone)) {
            mUserEducationHelper.requestShowIph(
                    buttonData.getButtonSpec().getIphCommandBuilder().build());
        }
    }

    private void hideCurrentOptionalButton() {
        if (mDelegate != null) {
            mDelegate.setOptionalButtonData(null);
        }
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

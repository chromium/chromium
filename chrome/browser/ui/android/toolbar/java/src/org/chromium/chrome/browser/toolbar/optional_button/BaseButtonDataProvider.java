// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.content.res.Resources;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.CallSuper;

import org.chromium.base.FeatureList;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Base class for button data providers used on the adaptive toolbar. */
@NullMarked
public abstract class BaseButtonDataProvider implements ButtonDataProvider, OnClickListener {
    protected final ButtonDataImpl mButtonData;
    protected final Supplier<@Nullable Tab> mActiveTabSupplier;

    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private final @Nullable ModalDialogManager mModalDialogManager;
    private @Nullable ModalDialogManagerObserver mModalDialogObserver;

    private boolean mShouldShowOnIncognitoTabs;

    /**
     * Creates a new instance of {@code BaseButtonDataProvider}.
     *
     * @param activeTabSupplier Supplier for the current active tab.
     * @param modalDialogManager Modal dialog manager, used to disable the button when a dialog is
     *     visible. Can be null to disable this behavior.
     * @param buttonSpec ButtonSpec describing the button.
     */
    public BaseButtonDataProvider(
            Supplier<@Nullable Tab> activeTabSupplier,
            @Nullable ModalDialogManager modalDialogManager,
            ButtonSpec buttonSpec) {
        mActiveTabSupplier = activeTabSupplier;
        mModalDialogManager = modalDialogManager;
        if (mModalDialogManager != null) {
            mModalDialogObserver =
                    new ModalDialogManagerObserver() {
                        @Override
                        public void onDialogAdded(PropertyModel model) {
                            mButtonData.setEnabled(false);
                            notifyObservers(mButtonData.canShow());
                        }

                        @Override
                        public void onLastDialogDismissed() {
                            mButtonData.setEnabled(true);
                            notifyObservers(mButtonData.canShow());
                        }
                    };
            mModalDialogManager.addObserver(mModalDialogObserver);
        }

        if (!AdaptiveToolbarFeatures.isDynamicAction(buttonSpec.getButtonVariant())) {
            assert buttonSpec.getActionChipLabelResId() == Resources.ID_NULL
                    : "Action chip should only be used on dynamic actions";
        }

        ButtonSpec specWithListener =
                new ButtonSpec.Builder(buttonSpec).setOnClickListener(this).build();
        mButtonData =
                new ButtonDataImpl(/* canShow= */ false, /* isEnabled= */ true, specWithListener);
    }

    /**
     * Checks if the button should be shown for the current tab. Called every time {@code get()} is
     * invoked.
     *
     * @param tab Current tab.
     * @return whether the button should be shown for the current tab.
     */
    @Contract("null -> false")
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (tab == null) return false;

        if (tab.isIncognito() && !mShouldShowOnIncognitoTabs) return false;

        return true;
    }

    protected void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    /**
     * Sets the button's {@link IphCommandBuilder} if needed, called every time {@code get()} is
     * invoked.
     *
     * @param tab Current tab.
     */
    private void maybeSetIphCommandBuilder(@Nullable Tab tab) {
        // State not ready.
        if (tab == null || !FeatureList.isInitialized()) {
            return;
        }

        // Button already has an override IPH builder.
        if (mButtonData.getButtonSpec().getIphCommandBuilder() != null) {
            return;
        }

        // Adaptive toolbar customization is disabled.
        if (!AdaptiveToolbarFeatures.isCustomizationEnabled()) {
            return;
        }

        // Do not show IPH if the action chip is shown with a valid label.
        if (AdaptiveToolbarFeatures.shouldShowActionChip(
                        mButtonData.getButtonSpec().getButtonVariant())
                && mButtonData.getButtonSpec().getActionChipLabelResId() != Resources.ID_NULL) {
            return;
        }

        mButtonData.updateIphCommandBuilder(getIphCommandBuilder(tab));
    }

    /** Sets whether the button should be shown on incognito tabs, default is false. */
    protected void setShouldShowOnIncognitoTabs(boolean shouldShowOnIncognitoTabs) {
        mShouldShowOnIncognitoTabs = shouldShowOnIncognitoTabs;
    }

    /**
     * Gets an {@link IphCommandBuilder} builder instance to use on this button. Only called when
     * native is initialized and when there's no IphCommandBuilder set.
     *
     * @param tab Current tab.
     * @return An {@link org.chromium.chrome.browser.user_education.IphCommand} instance to set on
     *     this button, or null if no IPH should be used.
     */
    protected @Nullable IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return null;
    }

    /** ButtonDataProvider implementation. */
    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        mButtonData.setCanShow(shouldShowButton(tab));
        maybeSetIphCommandBuilder(tab);

        return mButtonData;
    }

    @Override
    @CallSuper
    @SuppressWarnings("NullAway")
    public void destroy() {
        mObservers.clear();
        if (mModalDialogManager != null) {
            mModalDialogManager.removeObserver(mModalDialogObserver);
        }
    }

    /* OnClickListener implementation. */
    @Override
    public abstract void onClick(View view);
}

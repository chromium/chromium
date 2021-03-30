// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;

/** Meta {@link ButtonDataProvider} which chooses the optional button variant that will be shown. */
public class AdaptiveToolbarButtonController implements ButtonDataProvider, ButtonDataObserver {
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    @Nullable
    private ButtonDataProvider mSingleProvider;

    /** {@code true} if the SessionVariant histogram value was already recorded. */
    private boolean mIsSessionVariantRecorded;

    /** The last received {@link ButtonSpec}. */
    @Nullable
    private ButtonSpec mOriginalButtonSpec;

    /**
     * {@link ButtonData} instance returned by {@link AdaptiveToolbarButtonController#get(Tab)}
     * when wrapping {@code mOriginalButtonSpec}.
     */
    private final ButtonDataImpl mButtonData = new ButtonDataImpl();

    /**
     * Adds an instance of a button variant to the collection of buttons managed by {@code
     * AdaptiveToolbarButtonController}.
     *
     * @param variant The button variant of {@code buttonProvider}.
     * @param buttonProvider The provider implementing the button variant. {@code
     *         AdaptiveToolbarButtonController} takes ownership of the provider and will {@link
     *         #destroy()} it, once the provider is no longer needed.
     */
    public void addButtonVariant(
            @AdaptiveToolbarButtonVariant int variant, ButtonDataProvider buttonProvider) {
        assert AdaptiveToolbarFeatures.isEnabled() : "Adaptive toolbar button is disabled";
        assert variant >= 0
                && variant < AdaptiveToolbarButtonVariant.NUM_ENTRIES
            : "invalid adaptive button variant: "
                                + variant;
        assert variant
                != AdaptiveToolbarButtonVariant.UNKNOWN
            : "must not provide UNKNOWN button provider";
        assert variant
                != AdaptiveToolbarButtonVariant.NONE : "must not provide NONE button provider";

        if (variant != AdaptiveToolbarFeatures.getSingleVariantMode()) {
            buttonProvider.destroy();
            return;
        }

        setSingleProvider(buttonProvider);
    }

    @Override
    public void destroy() {
        setSingleProvider(null);
        mObservers.clear();
    }

    private void setSingleProvider(@Nullable ButtonDataProvider buttonProvider) {
        if (mSingleProvider != null) {
            mSingleProvider.removeObserver(this);
            mSingleProvider.destroy();
        }
        mSingleProvider = buttonProvider;
        if (mSingleProvider != null) {
            mSingleProvider.addObserver(this);
        }
    }

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
        if (mSingleProvider == null) return null;
        final ButtonData receivedButtonData = mSingleProvider.get(tab);
        if (receivedButtonData == null) return null;
        assert receivedButtonData.getButtonSpec().getButtonVariant()
                == AdaptiveToolbarFeatures.getSingleVariantMode();

        if (!mIsSessionVariantRecorded && receivedButtonData.canShow()
                && receivedButtonData.isEnabled()) {
            mIsSessionVariantRecorded = true;
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.AdaptiveToolbarButton.SessionVariant",
                    receivedButtonData.getButtonSpec().getButtonVariant(),
                    AdaptiveToolbarButtonVariant.NUM_ENTRIES);
        }
        // Return early if there is no need to wrap the listener.
        if (receivedButtonData.getButtonSpec().getOnClickListener() == null) {
            return receivedButtonData;
        }

        mButtonData.setCanShow(receivedButtonData.canShow());
        mButtonData.setEnabled(receivedButtonData.isEnabled());
        final ButtonSpec receivedButtonSpec = receivedButtonData.getButtonSpec();
        // ButtonSpec is immutable, so we keep the previous value when noting changes.
        if (receivedButtonSpec != mOriginalButtonSpec) {
            mOriginalButtonSpec = receivedButtonSpec;
            mButtonData.setButtonSpec(new ButtonSpec(receivedButtonSpec.getDrawable(),
                    wrapListener(receivedButtonSpec.getOnClickListener(),
                            receivedButtonSpec.getButtonVariant()),
                    receivedButtonSpec.getContentDescriptionResId(),
                    receivedButtonSpec.getSupportsTinting(),
                    receivedButtonSpec.getIPHCommandBuilder(),
                    receivedButtonSpec.getButtonVariant()));
        }
        return mButtonData;
    }

    private static View.OnClickListener wrapListener(View.OnClickListener receivedListener,
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        return view -> {
            RecordHistogram.recordEnumeratedHistogram("Android.AdaptiveToolbarButton.Clicked",
                    buttonVariant, AdaptiveToolbarButtonVariant.NUM_ENTRIES);
            receivedListener.onClick(view);
        };
    }

    @Override
    public void buttonDataChanged(boolean canShowHint) {
        notifyObservers(canShowHint);
    }

    private void notifyObservers(boolean canShowHint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(canShowHint);
        }
    }
}

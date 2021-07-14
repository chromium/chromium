// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarPreferenceFragment;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/** Meta {@link ButtonDataProvider} which chooses the optional button variant that will be shown. */
public class AdaptiveToolbarButtonController implements ButtonDataProvider, ButtonDataObserver,
                                                        NativeInitObserver,
                                                        SharedPreferencesManager.Observer {
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    @Nullable
    private ButtonDataProvider mSingleProvider;

    // Maps from {@link AdaptiveToolbarButtonVariant} to {@link ButtonDataProvider}.
    private Map<Integer, ButtonDataProvider> mButtonDataProviderMap = new HashMap<>();

    /**
     * {@link ButtonData} instance returned by {@link AdaptiveToolbarButtonController#get(Tab)}
     * when wrapping {@code mOriginalButtonSpec}.
     */
    private final ButtonDataImpl mButtonData = new ButtonDataImpl();

    /** The last received {@link ButtonSpec}. */
    @Nullable
    private ButtonSpec mOriginalButtonSpec;

    /** {@code true} if the SessionVariant histogram value was already recorded. */
    private boolean mIsSessionVariantRecorded;

    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final SharedPreferencesManager mSharedPreferencesManager;

    @Nullable
    private View.OnLongClickListener mMenuHandler;
    private final Callback<Integer> mMenuClickListener;
    private final AdaptiveButtonActionMenuCoordinator mMenuCoordinator;

    /**
     * Constructs the {@link AdaptiveToolbarButtonController}.
     *
     * @param context used in {@link SettingsLauncher}
     * @param settingsLauncher opens adaptive button settings
     * @param lifecycleDispatcher notifies about native initialization
     */
    public AdaptiveToolbarButtonController(Context context, SettingsLauncher settingsLauncher,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            AdaptiveButtonActionMenuCoordinator menuCoordinator,
            SharedPreferencesManager sharedPreferencesManager) {
        mMenuClickListener = id -> {
            if (id == R.id.customize_adaptive_button_menu_id) {
                RecordUserAction.record("MobileAdaptiveMenuCustomize");
                settingsLauncher.launchSettingsActivity(
                        context, AdaptiveToolbarPreferenceFragment.class);
                return;
            }
            assert false : "unknown adaptive button menu id: " + id;
        };
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mMenuCoordinator = menuCoordinator;
        mSharedPreferencesManager = sharedPreferencesManager;
        mSharedPreferencesManager.addObserver(this);
    }

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
        assert variant >= 0
                && variant < AdaptiveToolbarButtonVariant.NUM_ENTRIES
            : "invalid adaptive button variant: "
                                + variant;
        assert variant
                != AdaptiveToolbarButtonVariant.UNKNOWN
            : "must not provide UNKNOWN button provider";
        assert variant
                != AdaptiveToolbarButtonVariant.NONE : "must not provide NONE button provider";

        mButtonDataProviderMap.put(variant, buttonProvider);
    }

    @Override
    public void destroy() {
        setSingleProvider(null);
        mObservers.clear();
        mSharedPreferencesManager.removeObserver(this);
        mLifecycleDispatcher.unregister(this);

        Iterator<Map.Entry<Integer, ButtonDataProvider>> it =
                mButtonDataProviderMap.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<Integer, ButtonDataProvider> entry = it.next();
            entry.getValue().destroy();
            it.remove();
        }
    }

    private void setSingleProvider(@Nullable ButtonDataProvider buttonProvider) {
        if (mSingleProvider != null) {
            mSingleProvider.removeObserver(this);
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

        if (!mIsSessionVariantRecorded && receivedButtonData.canShow()
                && receivedButtonData.isEnabled()) {
            mIsSessionVariantRecorded = true;
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.AdaptiveToolbarButton.SessionVariant",
                    receivedButtonData.getButtonSpec().getButtonVariant(),
                    AdaptiveToolbarButtonVariant.NUM_ENTRIES);
        }

        mButtonData.setCanShow(receivedButtonData.canShow());
        mButtonData.setEnabled(receivedButtonData.isEnabled());
        final ButtonSpec receivedButtonSpec = receivedButtonData.getButtonSpec();
        // ButtonSpec is immutable, so we keep the previous value when noting changes.
        if (receivedButtonSpec != mOriginalButtonSpec) {
            assert receivedButtonSpec.getOnLongClickListener()
                    == null
                : "adaptive button variants are expected to not set a long click listener";
            if (mMenuHandler == null) mMenuHandler = createMenuHandler();
            mOriginalButtonSpec = receivedButtonSpec;
            mButtonData.setButtonSpec(new ButtonSpec(receivedButtonSpec.getDrawable(),
                    wrapClickListener(receivedButtonSpec.getOnClickListener(),
                            receivedButtonSpec.getButtonVariant()),
                    mMenuHandler, receivedButtonSpec.getContentDescriptionResId(),
                    receivedButtonSpec.getSupportsTinting(),
                    receivedButtonSpec.getIPHCommandBuilder(),
                    receivedButtonSpec.getButtonVariant()));
        }
        return mButtonData;
    }

    private static View.OnClickListener wrapClickListener(View.OnClickListener receivedListener,
            @AdaptiveToolbarButtonVariant int buttonVariant) {
        return view -> {
            RecordHistogram.recordEnumeratedHistogram("Android.AdaptiveToolbarButton.Clicked",
                    buttonVariant, AdaptiveToolbarButtonVariant.NUM_ENTRIES);
            receivedListener.onClick(view);
        };
    }

    @Nullable
    private View.OnLongClickListener createMenuHandler() {
        if (!FeatureList.isInitialized()) return null;
        return mMenuCoordinator.createOnLongClickListener(mMenuClickListener);
    }

    @Override
    public void buttonDataChanged(boolean canShowHint) {
        notifyObservers(canShowHint);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (AdaptiveToolbarFeatures.isSingleVariantModeEnabled()) {
            @AdaptiveToolbarButtonVariant
            int variant = AdaptiveToolbarFeatures.getSingleVariantMode();
            setSingleProvider(mButtonDataProviderMap.get(variant));
        } else if (AdaptiveToolbarFeatures.isCustomizationEnabled()) {
            new AdaptiveToolbarStatePredictor().recomputeUiState(uiState -> {
                setSingleProvider(uiState.canShowUi
                                ? mButtonDataProviderMap.get(uiState.toolbarButtonState)
                                : null);
                notifyObservers(uiState.canShowUi);
            });
            // We need the menu handler only if the customization feature is on.
            if (mMenuHandler != null) return;
            mMenuHandler = createMenuHandler();
            if (mMenuHandler == null) return;
        } else {
            return;
        }

        // Clearing mOriginalButtonSpec forces a refresh of mButtonData on the next get()
        mOriginalButtonSpec = null;
        notifyObservers(mButtonData.canShow());
    }

    private void notifyObservers(boolean canShowHint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(canShowHint);
        }
    }

    /** Returns the {@link ButtonDataProvider} used in a single-variant mode. */
    @Nullable
    @VisibleForTesting
    public ButtonDataProvider getSingleProviderForTesting() {
        return mSingleProvider;
    }

    @Override
    public void onPreferenceChanged(String key) {
        if (ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS.equals(key)
                || ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED.equals(key)) {
            assert AdaptiveToolbarFeatures.isCustomizationEnabled();
            new AdaptiveToolbarStatePredictor().recomputeUiState(uiState -> {
                setSingleProvider(uiState.canShowUi
                                ? mButtonDataProviderMap.get(uiState.toolbarButtonState)
                                : null);
                notifyObservers(uiState.canShowUi);
            });
        }
    }
}

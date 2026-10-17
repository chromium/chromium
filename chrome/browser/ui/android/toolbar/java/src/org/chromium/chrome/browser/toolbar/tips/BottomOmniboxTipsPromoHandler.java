// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tips.TipsPromoHandler;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.tips.TipsSignalKeys;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.Map;

/**
 * {@link TipsPromoHandler} for the Bottom Omnibox tip. Supplies the promo bottom sheet content,
 * opens the address bar settings with the bottom toolbar option highlighted when the promo is
 * accepted, and reports whether the omnibox is currently at the bottom as an eligibility signal.
 */
@NullMarked
public class BottomOmniboxTipsPromoHandler implements TipsPromoHandler {
    @Override
    public FeatureTipPromoData getPromoData(Context context) {
        return new FeatureTipPromoData(
                context.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                context.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox),
                context.getString(R.string.tips_promo_bottom_sheet_description_bottom_omnibox),
                R.drawable.tips_promo_bottom_omnibox_logo,
                context.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short),
                List.of(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox),
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox),
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox)));
    }

    @Override
    public void onPromoAccepted(WindowAndroid windowAndroid) {
        // The hosting activity outlives the promo in practice, but bail out rather than crash if
        // its context has already been released.
        Context context = windowAndroid.getContext().get();
        if (context == null) return;
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        context,
                        AddressBarSettingsFragment.class,
                        AddressBarSettingsFragment.createArguments(
                                HighlightedOption.BOTTOM_TOOLBAR));
    }

    @Override
    public Map<String, Float> getCustomSignals(WindowAndroid windowAndroid) {
        return Map.of(
                TipsSignalKeys.BOTTOM_OMNIBOX_STATUS,
                isBottomOmniboxActive(windowAndroid) ? 1.0f : 0.0f);
    }

    /**
     * Returns whether the browser controls are bottom-anchored, defaulting to false (top) when no
     * {@link BrowserControlsStateProvider} is available.
     */
    private static boolean isBottomOmniboxActive(WindowAndroid windowAndroid) {
        BrowserControlsStateProvider provider =
                BrowserControlsStateProviderSupplier.getValueOrNullFrom(windowAndroid);
        return provider != null && provider.getControlsPosition() == ControlsPosition.BOTTOM;
    }
}

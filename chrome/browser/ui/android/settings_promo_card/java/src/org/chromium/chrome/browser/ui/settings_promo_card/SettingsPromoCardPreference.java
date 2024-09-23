// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.Tracker;

/** A preference that displays a settings promo card. */
public class SettingsPromoCardPreference extends Preference {
    @Nullable private SettingsPromoCardProvider mProvider;

    /** Construct and initialize SettingsPromoCardPreference to be shown in main settings. */
    public SettingsPromoCardPreference(Context context, AttributeSet attrs, Tracker tracker) {
        super(context, attrs);
        setLayoutResource(R.layout.settings_promo_card);

        mProvider =
                new DefaultBrowserPromoCard(
                        context,
                        DefaultBrowserPromoUtils.getInstance(),
                        tracker,
                        this::onPromoCardUpdated);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        setVisible(false);
        if (mProvider != null) {
            ViewGroup container = (ViewGroup) holder.itemView;
            container.removeAllViews();
            container.addView(mProvider.getView());
            setVisible(mProvider.isPromoShowing());
        }
    }

    /**
     * Update the preference and the content when the settings fragment is updating. (e.g.
     * onResume).
     */
    public void updatePreferences() {
        if (mProvider != null) {
            mProvider.updatePromoCard();
        }
    }

    private void onPromoCardUpdated() {
        setVisible(mProvider != null && mProvider.isPromoShowing());
        notifyChanged();
    }
}

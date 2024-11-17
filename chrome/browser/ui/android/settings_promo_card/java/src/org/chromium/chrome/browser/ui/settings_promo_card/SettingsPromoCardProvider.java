// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import android.view.View;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The interface for a card shown in the settings promo card. */
public interface SettingsPromoCardProvider {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.PROMO_HIDDEN, State.PROMO_SHOWING})
    public @interface State {
        int PROMO_HIDDEN = 0;
        int PROMO_SHOWING = 1;
    }

    /**
     * Sets up the promo card view.
     *
     * @param view The {@link PromoCardView} that should be set up.
     */
    void setUpPromoCardView(View view);

    /**
     * Gets the {@link View} that holds the content to be displayed in the settings promo card.
     *
     * @return The content view.
     */
    View getView();

    /**
     * @return whether the promo card should be showing or hiding.
     */
    boolean isPromoShowing();

    /** Update the promo card content when the settings fragment is updating. (e.g. on onResume). */
    void updatePromoCard();
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.settings_promo_card.SettingsPromoCardProvider.State;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Controller for Default browser settings promo card when Chrome is not the default browser
 * TODO(crbug.com/364906215): Maybe migrate this to use MVC-ed `PromoCardCoordiantor`.
 */
public class DefaultBrowserPromoCard implements SettingsPromoCardProvider {
    private final Context mContext;
    private final DefaultBrowserPromoUtils mPromoUtils;
    private final Tracker mTracker;
    private final Runnable mOnDisplayStateChanged;

    private final View mView;

    private @State int mState = State.PROMO_HIDDEN;

    /**
     * Construct and initialize DefaultBrowserPromoCard view, to be added to the
     * SettingsPromoCardPreference.
     */
    public DefaultBrowserPromoCard(
            Context context,
            DefaultBrowserPromoUtils promoUtils,
            Tracker tracker,
            Runnable onDisplayStateChanged) {
        mContext = context;
        mPromoUtils = promoUtils;
        mTracker = tracker;
        mOnDisplayStateChanged = onDisplayStateChanged;

        if (mPromoUtils.shouldShowNonRoleManagerPromo(context)
                && tracker.shouldTriggerHelpUI(
                        FeatureConstants.DEFAULT_BROWSER_PROMO_SETTING_CARD)) {
            mState = State.PROMO_SHOWING;
        }
        mView = LayoutInflater.from(mContext).inflate(R.layout.default_browser_settings_view, null);
        mView.findViewById(R.id.open_settings_button).setOnClickListener(this::onPromoClicked);
        mView.findViewById(R.id.close_button).setOnClickListener(this::onDismissClicked);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public boolean isPromoShowing() {
        return mState == State.PROMO_SHOWING;
    }

    @Override
    public void updatePromoCard() {
        if (!mPromoUtils.shouldShowNonRoleManagerPromo(mContext)) {
            setPromoHidden();
        }
    }

    private void onPromoClicked(View view) {
        mTracker.notifyEvent("default_browser_promo_setting_card_used");

        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.safeStartActivity(mContext, intent);
    }

    private void onDismissClicked(View view) {
        mTracker.notifyEvent("default_browser_promo_setting_card_dismissed");
        setPromoHidden();
    }

    private void setPromoHidden() {
        if (mState != State.PROMO_HIDDEN) {
            mState = State.PROMO_HIDDEN;
            mOnDisplayStateChanged.run();
        }
    }
}

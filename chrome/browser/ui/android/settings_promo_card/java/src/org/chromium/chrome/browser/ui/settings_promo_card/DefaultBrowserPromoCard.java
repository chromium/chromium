// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.settings_promo_card.SettingsPromoCardProvider.State;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator;
import org.chromium.components.browser_ui.widget.promo.PromoCardProperties;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/** Controller for Default browser settings promo card when Chrome is not the default browser */
public class DefaultBrowserPromoCard implements SettingsPromoCardProvider {
    private final Context mContext;
    private final DefaultBrowserPromoUtils mPromoUtils;
    private final Tracker mTracker;
    private final Runnable mOnDisplayStateChanged;

    private @State int mState = State.PROMO_HIDDEN;
    @Nullable private PromoCardCoordinator mCardCoordinator;

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
    }

    @Override
    public void setUpPromoCardView(View view) {
        assert mState == State.PROMO_SHOWING;

        mCardCoordinator =
                PromoCardCoordinator.createFromView(
                        view, buildPropertyModel(), "default-browser-promo");
    }

    private PropertyModel buildPropertyModel() {
        return new PropertyModel.Builder(PromoCardProperties.ALL_KEYS)
                .with(
                        PromoCardProperties.IMAGE,
                        ApiCompatibilityUtils.getDrawable(
                                mContext.getResources(),
                                R.drawable.default_browser_promo_illustration))
                .with(
                        PromoCardProperties.TITLE,
                        mContext.getResources()
                                .getString(R.string.default_browser_promo_card_title))
                .with(
                        PromoCardProperties.DESCRIPTION,
                        mContext.getResources()
                                .getString(R.string.default_browser_promo_card_description))
                .with(
                        PromoCardProperties.PRIMARY_BUTTON_TEXT,
                        mContext.getResources()
                                .getString(R.string.default_browser_promo_open_settings_label))
                .with(PromoCardProperties.BUTTONS_WIDTH, LayoutParams.WRAP_CONTENT)
                .with(PromoCardProperties.PRIMARY_BUTTON_CALLBACK, this::onPromoClicked)
                .with(PromoCardProperties.HAS_CLOSE_BUTTON, true)
                .with(PromoCardProperties.CLOSE_BUTTON_CALLBACK, this::onDismissClicked)
                .with(PromoCardProperties.HAS_SECONDARY_BUTTON, false)
                .build();
    }

    @Override
    public View getView() {
        return mCardCoordinator.getView();
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
            if (mCardCoordinator != null) mCardCoordinator.destroy();
        }
    }
}

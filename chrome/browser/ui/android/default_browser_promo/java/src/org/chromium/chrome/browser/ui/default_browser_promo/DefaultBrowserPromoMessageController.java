// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;

/** A controller class for messages that promo user to set Chrome as the default browser. */
public class DefaultBrowserPromoMessageController {
    private final Context mContext;
    private final Tracker mTracker;

    /**
     * @param context context to show promo dialogs.
     * @param tracker The {@link Tracker}
     */
    public DefaultBrowserPromoMessageController(Context context, Tracker tracker) {
        mContext = context;
        mTracker = tracker;
    }

    /** Construct the PropertyModel and enqueue the default browser promo message. */
    void promo(MessageDispatcher dispatcher) {
        dispatcher.enqueueWindowScopedMessage(buildPropertyModel(), false);
    }

    @VisibleForTesting
    PropertyModel buildPropertyModel() {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(
                        MessageBannerProperties.MESSAGE_IDENTIFIER,
                        MessageIdentifier.DEFAULT_BROWSER_PROMO)
                .with(
                        MessageBannerProperties.TITLE,
                        mContext.getString(R.string.default_browser_promo_message_title))
                .with(
                        MessageBannerProperties.DESCRIPTION,
                        mContext.getString(R.string.default_browser_promo_message_description))
                .with(
                        MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                        mContext.getString(R.string.default_browser_promo_message_settings_button))
                .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_chrome)
                .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::onPrimaryAction)
                .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                .build();
    }

    @VisibleForTesting
    @PrimaryActionClickBehavior
    int onPrimaryAction() {
        mTracker.notifyEvent("default_browser_promo_messages_used");

        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.safeStartActivity(mContext, intent);

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        if (dismissReason == DismissReason.GESTURE) {
            mTracker.notifyEvent("default_browser_promo_messages_dismissed");
        }
    }
}

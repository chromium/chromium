// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetReceiver}. This class
 * contains as much of the business logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetReceiverDelegate {
    private final ComponentName mSearchComponent;
    private final ComponentName mChromeLauncherComponent;

    // These are the actions that the QuickActionSearchWidgetReceiver will subscribe to
    // in the AndroidManifest.xml. Keep these values in sync with the values found in the
    // AndroidManifest.
    static final String ACTION_START_TEXT_QUERY =
            "org.chromium.chrome.browser.ui.quickactionsearchwidget.START_TEXT_QUERY";
    static final String ACTION_START_VOICE_QUERY =
            "org.chromium.chrome.browser.ui.quickactionsearchwidget.START_VOICE_QUERY";
    static final String ACTION_START_DINO_GAME =
            "org.chromium.chrome.browser.ui.quickactionsearchwidget.START_DINO_GAME";

    /**
     * Constructor for the {@link QuickActionSearchWidgetReceiverDelegate}
     *
     * @param searchComponent The component that will be launched when ACTION_START_TEXT_QUERY is
     *         received. Generally this component is {@link SearchActivity}.
     * @param chromeLauncherComponent The component that will be used to dispatch intents to the
     *         appropriate Chrome activities. Generally this component is {@link
     *         ChromeLauncherActivity}.
     */
    public QuickActionSearchWidgetReceiverDelegate(
            ComponentName searchComponent, ComponentName chromeLauncherComponent) {
        mSearchComponent = searchComponent;
        mChromeLauncherComponent = chromeLauncherComponent;
    }

    /**
     * Handles the intent actions sent to the widget.
     *
     * @param context The {@link Context} in which the QuickActionSearchWidgetReceiver is running.
     * @param intent  The {@link Intent} that specifies which quick action is being received.
     */
    public void handleAction(final Context context, final Intent intent) {
        String action = intent.getAction();
        if (ACTION_START_TEXT_QUERY.equals(action)) {
            startSearchActivity(context, /*shouldStartVoiceSearch=*/false);
        } else if (ACTION_START_VOICE_QUERY.equals(action)) {
            startSearchActivity(context, /*shouldStartVoiceSearch=*/true);
        } else if (ACTION_START_DINO_GAME.equals(action)) {
            startDinoGame(context);
        } else {
            assert false : "Unsupported QuickActionSearchWidget action";
        }
    }

    /**
     * Starts the component specified by mSearchComponent. Generally this component is {@link
     * SearchActivity}.
     *
     * @param context The {@link Context} in which we will launch the activity.
     * @param shouldStartVoiceSearch If the activity should be launched in voice search mode.
     */
    private void startSearchActivity(final Context context, boolean shouldStartVoiceSearch) {
        Intent searchIntent = new Intent();
        searchIntent.setComponent(mSearchComponent);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        searchIntent.putExtra(
                SearchActivityConstants.EXTRA_SHOULD_START_VOICE_SEARCH, shouldStartVoiceSearch);

        Bundle optionsBundle =
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle();
        IntentUtils.safeStartActivity(context, searchIntent, optionsBundle);
    }

    /**
     * Launches a new tab with chrome://dino URL.
     *
     * @param context the {@link Context} in which we will launch the activity.
     */
    private void startDinoGame(final Context context) {
        Intent intent = createDinoIntent(context);

        IntentUtils.safeStartActivity(context, intent,
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle());
    }

    /**
     * Creates an intent to launch a new tab with chrome://dino/ URL.
     *
     * @param context The context from which the intent is being created.
     * @return An intent to launch a tab with a new tab with chrome://dino/ URL.
     */
    private Intent createDinoIntent(final Context context) {
        // We concatenate the forward slash to the URL since if a Dino tab already exists, we would
        // like to reuse it. In order to determine if there is an existing Dino tab,
        // ChromeTabbedActivity will check by comparing URLs of existing tabs to the URL of our
        // intent. If there is an existing Dino tab, it would have a forward slash appended to the
        // end of its URL, so our URL must have a forward slash to match.
        String chromeDinoUrl = UrlConstants.CHROME_DINO_URL + "/";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(chromeDinoUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setComponent(mChromeLauncherComponent);
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);

        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }
}

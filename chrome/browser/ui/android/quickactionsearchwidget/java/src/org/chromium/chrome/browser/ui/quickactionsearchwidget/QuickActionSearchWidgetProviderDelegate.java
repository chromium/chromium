// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.widget.RemoteViews;

import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.IntentUtils;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the business logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    private final ComponentName mSearchComponent;
    private final ComponentName mWidgetComponent;

    // These are the actions that the QuickActionSearchWidgetProvider will subscribe to
    // in the AndroidManifest.xml. Keep these values in sync with the values found in the
    // AndroidManifest.
    static final String ACTION_START_TEXT_QUERY =
            "org.chromium.chrome.browser.ui.quickactionsearchwidget.START_TEXT_QUERY";

    /**
     * Constructor for the {@link QuickActionSearchWidgetProviderDelegate}
     *
     * @param searchComponent The component that will be launched when ACTION_START_TEXT_QUERY is
     *                        received. Generally this component is {@link SearchActivity}.
     * @param widgetComponent The component that is the AppWidgetProvider for the widget. See
     *                        {@link QuickActionSearchWidgetProvider}.
     */
    public QuickActionSearchWidgetProviderDelegate(
            ComponentName searchComponent, ComponentName widgetComponent) {
        mSearchComponent = searchComponent;
        mWidgetComponent = widgetComponent;
    }

    /**
     * Handles the intent actions sent to the widget.
     *
     * @param context The {@link Context} in which the QuickActionSearchWidgetProvider is running.
     * @param intent  The {@link Intent} that specifies which quick action is being received.
     */
    public void handleAction(final Context context, final Intent intent) {
        String action = intent.getAction();
        if (ACTION_START_TEXT_QUERY.equals(action)) {
            startSearchActivity(context);
        } else {
            assert false : "Unsupported QuickActionSearchWidget action";
        }
    }

    /**
     * Updates the RemoteViews of each widget. This is where the PendingIntents are assigned to
     * each button/surface of the widget.
     *
     * @param context   The {@link Context} in which the QuickActionSearchWidgetProvider is
     *                  running.
     * @param manager   An {@link AppWidgetManager} object that we will use to update our widgets.
     *                  See {@link AppWidgetManager#updateAppWidget}.
     * @param widgetIds The identifiers of the widgets for which an update is needed. Note that this
     *                  may be all of the AppWidget instances for the QuickActionSearch, or just a
     *                  subset of them.
     */
    public void updateWidgets(
            final Context context, final AppWidgetManager manager, final int[] widgetIds) {
        if (widgetIds == null) return;

        for (int i = 0; i < widgetIds.length; i++) {
            int widgetId = widgetIds[i];
            RemoteViews remoteViews = createWidgetRemoteViews(context);
            updateWidget(manager, widgetId, remoteViews);
        }
    }

    /**
     * Updates the RemoteViews of a given widget. We override this in our test classes in order to
     * keep a reference of the RemoteViews for tests.
     *
     * @param manager     An {@link AppWidgetManager} object that we will use to update our widgets.
     *                    See {@link AppWidgetManager#updateAppWidget}.
     * @param widgetId    The identifier of the widget the is to be updated.
     * @param remoteViews The updated {@link RemoteViews} that are being assigned to the widget
     *                    being updated.
     */
    @CallSuper
    protected void updateWidget(
            final AppWidgetManager manager, final int widgetId, final RemoteViews remoteViews) {
        manager.updateAppWidget(widgetId, remoteViews);
    }

    /**
     * Starts the component specified by mSearchComponent. Generally this component is {@link
     * SearchActivity}.
     *
     * @param context the {@link Context} in which we will launch the activity.
     */
    private void startSearchActivity(final Context context) {
        Intent searchIntent = new Intent();
        searchIntent.setComponent(mSearchComponent);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        Bundle optionsBundle =
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle();
        IntentUtils.safeStartActivity(context, searchIntent, optionsBundle);
    }

    /**
     * Creates a {@link RemoteViews} to be assigned to a widget in {@link
     * QuickActionSearchWidgetProviderDelegate#updateWidget(AppWidgetManager, int, RemoteViews)}. In
     * this function, the appropriate {@link PendingIntent} is assigned to each tap target on the
     * widget.
     *
     * @param context the {@link Context} from which the widget is being updated.
     */
    private RemoteViews createWidgetRemoteViews(final Context context) {
        RemoteViews remoteViews = new RemoteViews(
                context.getPackageName(), R.layout.quick_action_search_widget_layout);

        // Search Bar Intent
        Intent textSearchIntent = createTrustedIntentForAction(ACTION_START_TEXT_QUERY);
        remoteViews.setOnClickPendingIntent(R.id.quick_action_search_widget_search_bar_container,
                PendingIntent.getBroadcast(context, /*requestCode=*/0, textSearchIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT
                                | IntentUtils.getPendingIntentMutabilityFlag(false)));

        return remoteViews;
    }

    /**
     * Creates a trusted intent corresponding to a specified action.
     *
     * @param action a String specifying the action for the trusted intent.
     * @return A trusted intent corresponding to the specified action.
     */
    private Intent createTrustedIntentForAction(final String action) {
        Intent intent = new Intent(action);
        intent.setComponent(mWidgetComponent);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }
}
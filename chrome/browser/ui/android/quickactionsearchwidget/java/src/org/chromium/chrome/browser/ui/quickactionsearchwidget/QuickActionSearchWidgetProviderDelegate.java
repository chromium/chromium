// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.CallSuper;
import android.widget.RemoteViews;

import org.chromium.base.IntentUtils;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the widget logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    private final ComponentName mWidgetReceiverComponent;

    /**
     * Constructor for the {@link QuickActionSearchWidgetProviderDelegate}
     *
     * @param widgetReceiverComponent The {@link ComponentName} for the {@link
     *         android.content.BroadcastReceiver} that will receive the intents that are broadcast
     *         when the user interacts with the widget. Generally this component is {@link
     *         QuickActionSearchWidgetReceiver}.
     */
    public QuickActionSearchWidgetProviderDelegate(ComponentName widgetReceiverComponent) {
        mWidgetReceiverComponent = widgetReceiverComponent;
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
     * Creates a {@link RemoteViews} to be assigned to a widget in {@link
     * QuickActionSearchWidgetProviderDelegate#updateWidget(AppWidgetManager, int, RemoteViews)}. In
     * this function, the appropriate {@link PendingIntent} is assigned to each tap target on the
     * widget.
     * <p>
     * When the user taps on the widget, these PendingIntents are broadcast and are then
     * received by {@link QuickActionSearchWidgetReceiver}. QuickActionSearchWidgetReceiver then
     * calls {@link QuickActionSearchWidgetReceiverDelegate#handleAction} which invokes the
     * appropriate logic for the target that was tapped.
     *
     * @param context the {@link Context} from which the widget is being updated.
     */
    private RemoteViews createWidgetRemoteViews(final Context context) {
        RemoteViews remoteViews = new RemoteViews(
                context.getPackageName(), R.layout.quick_action_search_widget_layout);

        // Search Bar Intent
        PendingIntent textSearchPendingIntent = createPendingIntentForAction(
                QuickActionSearchWidgetReceiverDelegate.ACTION_START_TEXT_QUERY, context);
        remoteViews.setOnClickPendingIntent(
                R.id.quick_action_search_widget_search_bar_container, textSearchPendingIntent);

        // Voice Search Intent
        PendingIntent voiceSearchPendingIntent = createPendingIntentForAction(
                QuickActionSearchWidgetReceiverDelegate.ACTION_START_VOICE_QUERY, context);
        remoteViews.setOnClickPendingIntent(
                R.id.voice_search_quick_action_button, voiceSearchPendingIntent);

        // Dino Game intent
        PendingIntent dinoGamePendingIntent = createPendingIntentForAction(
                QuickActionSearchWidgetReceiverDelegate.ACTION_START_DINO_GAME, context);
        remoteViews.setOnClickPendingIntent(R.id.dino_quick_action_button, dinoGamePendingIntent);

        return remoteViews;
    }

    /**
     * Creates a {@link PendingIntent} that will broadcast a trusted intent for a specified action.
     *
     * @param context The Context from which the PendingIntent will perform the broadcast.
     * @param action A String specifying the action for the intent.
     * @return A {@link PendingIntent} that will broadcast a trusted intent for the specified
     *         action.
     */
    private PendingIntent createPendingIntentForAction(final String action, final Context context) {
        Intent intent = createTrustedIntentForAction(action);
        return PendingIntent.getBroadcast(context, /*requestCode=*/0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * Creates a trusted intent corresponding to a specified action.
     *
     * @param action a String specifying the action for the trusted intent.
     * @return A trusted intent corresponding to the specified action.
     */
    private Intent createTrustedIntentForAction(final String action) {
        Intent intent = new Intent(action);
        intent.setComponent(mWidgetReceiverComponent);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }
}
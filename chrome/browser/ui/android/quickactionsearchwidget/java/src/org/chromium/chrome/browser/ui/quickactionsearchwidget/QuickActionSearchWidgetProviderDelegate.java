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
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the widget logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    private final @QuickActionSearchWidgetType int mWidgetType;
    private final ComponentName mSearchActivityComponent;
    private final ComponentName mWidgetReceiverComponent;
    private final Intent mStartIncognitoTabIntent;

    /**
     * Constructor for the {@link QuickActionSearchWidgetProviderDelegate}
     *
     * @param widgetType
     * @param widgetReceiverComponent The {@link ComponentName} for the {@link
     *         android.content.BroadcastReceiver} that will receive the intents that are broadcast
     *         when the user interacts with the widget. Generally this component is {@link
     *         QuickActionSearchWidgetReceiver}.
     * @param searchActivityComponent Component linking to SearchActivity where all Search related
     *          events will be propagated.
     * @param startIncognitoIntent A trusted intent starting a new Incognito tab.
     */
    public QuickActionSearchWidgetProviderDelegate(@QuickActionSearchWidgetType int widgetType,
            ComponentName widgetReceiverComponent, ComponentName searchActivityComponent,
            Intent startIncognitoTabIntent) {
        mWidgetType = widgetType;
        mSearchActivityComponent = searchActivityComponent;
        mWidgetReceiverComponent = widgetReceiverComponent;
        mStartIncognitoTabIntent = startIncognitoTabIntent;
        mStartIncognitoTabIntent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
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
        int layoutId = getLayoutIdForWidgetType(mWidgetType);
        RemoteViews remoteViews = new RemoteViews(context.getPackageName(), layoutId);

        // Search Bar Intent
        PendingIntent textSearchPendingIntent = createPendingIntentForAction(
                context, SearchActivityConstants.ACTION_START_TEXT_SEARCH);
        remoteViews.setOnClickPendingIntent(
                R.id.quick_action_search_widget_search_bar_container, textSearchPendingIntent);

        // Voice Search Intent
        PendingIntent voiceSearchPendingIntent = createPendingIntentForAction(
                context, SearchActivityConstants.ACTION_START_VOICE_SEARCH);
        remoteViews.setOnClickPendingIntent(
                R.id.voice_search_quick_action_button, voiceSearchPendingIntent);

        // Incognito Tab Intent
        PendingIntent incognitoTabPendingIntent =
                createPendingIntent(context, mStartIncognitoTabIntent);
        remoteViews.setOnClickPendingIntent(
                R.id.incognito_quick_action_button, incognitoTabPendingIntent);

        // Lens Search Intent
        PendingIntent lensSearchPendingIntent = createPendingIntentForAction(
                context, SearchActivityConstants.ACTION_START_LENS_SEARCH);
        remoteViews.setOnClickPendingIntent(R.id.lens_quick_action_button, lensSearchPendingIntent);

        // Dino Game intent
        // TODO(crbug/1213541): Drop broadcast/intent trampoline and use direct launch intent.
        PendingIntent dinoGamePendingIntent = PendingIntent.getBroadcast(context, 0,
                createTrustedIntentForAction(mWidgetReceiverComponent,
                        QuickActionSearchWidgetReceiverDelegate.ACTION_START_DINO_GAME),
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
        remoteViews.setOnClickPendingIntent(R.id.dino_quick_action_button, dinoGamePendingIntent);

        return remoteViews;
    }

    /**
     * Creates a {@link PendingIntent} that will send a trusted intent with a specified action.
     *
     * @param context The Context from which the PendingIntent will perform the broadcast.
     * @param action A String specifying the action for the intent.
     * @return A {@link PendingIntent} that will broadcast a trusted intent for the specified
     *         action.
     */
    private PendingIntent createPendingIntentForAction(final Context context, final String action) {
        Intent intent = createTrustedIntentForAction(mSearchActivityComponent, action);
        return PendingIntent.getActivity(context, /*requestCode=*/0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * Creates a {@link PendingIntent} that will send a trusted intent with a specified action.
     *
     * @param context The Context from which the PendingIntent will perform the broadcast.
     * @param intent An intent to execute.
     * @return A {@link PendingIntent} that will broadcast a trusted intent for the specified
     *         action.
     */
    private PendingIntent createPendingIntent(final Context context, final Intent intent) {
        return PendingIntent.getActivity(context, /*requestCode=*/0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * Creates a trusted intent corresponding to a specified action.
     *
     * @param component The target component that will handle the action.
     * @param action a String specifying the action for the trusted intent.
     * @return A trusted intent corresponding to the specified action.
     */
    private Intent createTrustedIntentForAction(
            final ComponentName component, final String action) {
        Intent intent = new Intent(action);
        intent.setComponent(component);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    /**
     * Returns the layout resource id for a given {@link QuickActionSearchWidgetType}.
     *
     * @param widgetType A {@link QuickActionSearchWidgetType} that will correspond to a specific
     *         layout.
     * @return An int that is the resource id of the layout corresponding to the given widgetType.
     */
    private int getLayoutIdForWidgetType(@QuickActionSearchWidgetType int widgetType) {
        switch (widgetType) {
            case QuickActionSearchWidgetType.SMALL:
                return R.layout.quick_action_search_widget_small_layout;
            case QuickActionSearchWidgetType.MEDIUM:
                return R.layout.quick_action_search_widget_medium_layout;
            case QuickActionSearchWidgetType.DINO:
                return R.layout.quick_action_search_widget_dino_layout;
            case QuickActionSearchWidgetType.INVALID:
            default:
                assert false : "Unknown QuickActionSearchWidgetType";

                // If the case where we do not know which widget type to show,
                // we default to the small widget layout since it will fit in
                // whatever homescreen space is allocated to the widget.
                return R.layout.quick_action_search_widget_small_layout;
        }
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.view.View;
import android.widget.RemoteViews;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the widget logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    private final @LayoutRes int mLayoutRes;
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
    public QuickActionSearchWidgetProviderDelegate(@LayoutRes int layoutRes,
            @NonNull ComponentName widgetReceiverComponent,
            @NonNull ComponentName searchActivityComponent,
            @NonNull Intent startIncognitoTabIntent) {
        mLayoutRes = layoutRes;
        mSearchActivityComponent = searchActivityComponent;
        mWidgetReceiverComponent = widgetReceiverComponent;
        mStartIncognitoTabIntent = startIncognitoTabIntent;
        mStartIncognitoTabIntent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
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
     * @param context The {@link Context} from which the widget is being updated.
     * @param prefs Structure describing current preferences and feature availability.
     * @return Widget RemoteViews structure describing layout and content of the widget.
     */
    public RemoteViews createWidgetRemoteViews(
            @NonNull Context context, @NonNull SearchActivityPreferences prefs) {
        RemoteViews remoteViews = new RemoteViews(context.getPackageName(), mLayoutRes);

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
        remoteViews.setViewVisibility(R.id.voice_search_quick_action_button,
                prefs.voiceSearchAvailable ? View.VISIBLE : View.GONE);

        // Incognito Tab Intent
        PendingIntent incognitoTabPendingIntent =
                createPendingIntent(context, mStartIncognitoTabIntent);
        remoteViews.setOnClickPendingIntent(
                R.id.incognito_quick_action_button, incognitoTabPendingIntent);
        remoteViews.setViewVisibility(R.id.incognito_quick_action_button,
                prefs.incognitoAvailable ? View.VISIBLE : View.GONE);

        // Lens Search Intent
        PendingIntent lensSearchPendingIntent = createPendingIntentForAction(
                context, SearchActivityConstants.ACTION_START_LENS_SEARCH);
        remoteViews.setOnClickPendingIntent(R.id.lens_quick_action_button, lensSearchPendingIntent);
        remoteViews.setViewVisibility(R.id.lens_quick_action_button,
                prefs.googleLensAvailable ? View.VISIBLE : View.GONE);

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
    private PendingIntent createPendingIntentForAction(
            @NonNull Context context, @NonNull String action) {
        Intent intent = createTrustedIntentForAction(mSearchActivityComponent, action);
        intent.putExtra(
                SearchActivityConstants.EXTRA_BOOLEAN_FROM_QUICK_ACTION_SEARCH_WIDGET, true);
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
    private PendingIntent createPendingIntent(@NonNull Context context, @NonNull Intent intent) {
        intent.putExtra(
                SearchActivityConstants.EXTRA_BOOLEAN_FROM_QUICK_ACTION_SEARCH_WIDGET, true);
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
            @NonNull ComponentName component, @NonNull String action) {
        Intent intent = new Intent(action);
        intent.setComponent(component);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }
}

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
    private final Intent mStartIncognitoTabIntent;
    private final Intent mStartDinoGameIntent;

    /**
     * Constructor for the {@link QuickActionSearchWidgetProviderDelegate}
     *
     * @param widgetType
     * @param searchActivityComponent Component linking to SearchActivity where all Search related
     *          events will be propagated.
     * @param startIncognitoIntent A trusted intent starting a new Incognito tab.
     */
    public QuickActionSearchWidgetProviderDelegate(@LayoutRes int layoutRes,
            @NonNull ComponentName searchActivityComponent, @NonNull Intent startIncognitoTabIntent,
            @NonNull Intent startDinoGameIntent) {
        mLayoutRes = layoutRes;
        mSearchActivityComponent = searchActivityComponent;
        mStartIncognitoTabIntent = startIncognitoTabIntent;
        mStartDinoGameIntent = startDinoGameIntent;
    }

    /**
     * Creates a {@link RemoteViews} to be assigned to a widget in {@link
     * QuickActionSearchWidgetProviderDelegate#updateWidget(AppWidgetManager, int, RemoteViews)}. In
     * this function, the appropriate {@link PendingIntent} is assigned to each tap target on the
     * widget.
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
                context, SearchActivityConstants.ACTION_START_EXTENDED_TEXT_SEARCH);
        remoteViews.setOnClickPendingIntent(
                R.id.quick_action_search_widget_search_bar_container, textSearchPendingIntent);

        // Voice Search Intent
        PendingIntent voiceSearchPendingIntent = createPendingIntentForAction(
                context, SearchActivityConstants.ACTION_START_EXTENDED_VOICE_SEARCH);
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
        PendingIntent dinoGamePendingIntent = createPendingIntent(context, mStartDinoGameIntent);
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
        Intent intent = new Intent(action);
        intent.setComponent(mSearchActivityComponent);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);
        return createPendingIntent(context, intent);
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
}

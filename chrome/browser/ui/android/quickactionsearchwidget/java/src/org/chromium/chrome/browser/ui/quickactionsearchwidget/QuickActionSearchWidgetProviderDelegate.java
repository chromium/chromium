// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.view.View;
import android.widget.RemoteViews;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the widget logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    private final ComponentName mSearchActivityComponent;
    private final Intent mStartIncognitoTabIntent;
    private final Intent mStartDinoGameIntent;

    /**
     * @param searchActivityComponent Component linking to SearchActivity where all Search related
     *         events will be propagated.
     * @param startIncognitoTabIntent A trusted intent starting a new Incognito tab.
     * @param startDinoGameIntent A trusted intent starting the Dino game.
     */
    public QuickActionSearchWidgetProviderDelegate(@NonNull ComponentName searchActivityComponent,
            @NonNull Intent startIncognitoTabIntent, @NonNull Intent startDinoGameIntent) {
        mSearchActivityComponent = searchActivityComponent;
        mStartIncognitoTabIntent = startIncognitoTabIntent;
        mStartDinoGameIntent = startDinoGameIntent;
    }

    /**
     * Create {@link RemoteViews} for the Dino widget.
     *
     * @param context Current context.
     * @param prefs Structure describing current preferences and feature availability.
     * @return RemoteViews to be installed on the Dino widget.
     */
    public @NonNull RemoteViews createDinoWidgetRemoteViews(
            @NonNull Context context, @NonNull SearchActivityPreferences prefs) {
        return createWidgetRemoteViews(
                context, prefs, R.layout.quick_action_search_widget_dino_layout);
    }

    /**
     * Create configuration aware {@link RemoteViews} for the Search widget.
     *
     * The returned RemoteViews are adjusted to fit given space, and respond to
     * screen orientation changes.
     *
     * @param context Current context.
     * @param prefs Structure describing current preferences and feature availability.
     * @param portraitModeWidthDp Width of the widget area in portrait mode.
     * @param portraitModeHeightDp Height of the widget area in portrait mode.
     * @param landscapeModeWidthDp Width of the widget area in landscape mode.
     * @param landscapeModeHeightDp Height of the widget area in landscape mode.
     * @return RemoteViews to be installed on the Search widget.
     */
    public @NonNull RemoteViews createSearchWidgetRemoteViews(@NonNull Context context,
            @NonNull SearchActivityPreferences prefs, int portraitModeWidthDp,
            int portraitModeHeightDp, int landscapeModeWidthDp, int landscapeModeHeightDp) {
        return new RemoteViews(
                createWidgetRemoteViews(context, prefs,
                        getSearchWidgetLayoutForHeight(context, landscapeModeHeightDp)),
                createWidgetRemoteViews(context, prefs,
                        getSearchWidgetLayoutForHeight(context, portraitModeHeightDp)));
    }

    /**
     * Given height, identify the layout that will fit in the space.
     *
     * @param context Current context.
     * @param heightDp Are height in distance points.
     * @return Widget LayoutRes appropriate for the supplied height.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public @LayoutRes int getSearchWidgetLayoutForHeight(Context context, int heightDp) {
        Resources res = context.getResources();
        float density = res.getDisplayMetrics().density;

        float smallWidgetMinHeightDp =
                res.getDimension(R.dimen.quick_action_search_widget_small_height) / density;
        float mediumWidgetMinHeightDp =
                res.getDimension(R.dimen.quick_action_search_widget_medium_height) / density;

        if (heightDp < smallWidgetMinHeightDp) {
            return R.layout.quick_action_search_widget_xsmall_layout;
        } else if (heightDp < mediumWidgetMinHeightDp) {
            return R.layout.quick_action_search_widget_small_layout;
        }
        return R.layout.quick_action_search_widget_medium_layout;
    }

    /**
     * Create a {@link RemoteViews} from supplied layoutRes.
     * In this function, the appropriate {@link PendingIntent} is assigned to each tap target on the
     * widget.
     *
     * @param context The {@link Context} from which the widget is being updated.
     * @param prefs Structure describing current preferences and feature availability.
     * @param layoutRes The Layout to inflate.
     * @return Widget RemoteViews structure describing layout and content of the widget.
     */
    public RemoteViews createWidgetRemoteViews(@NonNull Context context,
            @NonNull SearchActivityPreferences prefs, @LayoutRes int layoutRes) {
        RemoteViews remoteViews = new RemoteViews(context.getPackageName(), layoutRes);

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

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.util.Size;
import android.view.View;
import android.widget.RemoteViews;

import androidx.annotation.DimenRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

/**
 * This class serves as the delegate for the {@link QuickActionSearchWidgetProvider}. This class
 * contains as much of the widget logic for the Quick Action Search Widget as possible.
 */
public class QuickActionSearchWidgetProviderDelegate {
    /** Class describing widget variant characteristics. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static class WidgetVariant {
        /** LayoutRes that describes this widget. */
        public final @LayoutRes int layout;

        /** The Reference width of the widget where all the content is visible. */
        public final int widgetWidthDp;

        /** The Reference height of the widget where all the content is visible. */
        public final int widgetHeightDp;

        /** The width of the button and surrounding margins. */
        public final int buttonWidthDp;

        /**
         * @param context The application context.
         * @param widthDimenRes The Resource ID describing reference width of the widget.
         * @param heightDimenRes The Resource ID describing reference height of the widget.
         * @param buttonWidthRes The Resource ID describing the width of the button.
         * @param buttonMarginRes The Resource ID describing the width of the button margins.
         */
        public WidgetVariant(
                Context context,
                @LayoutRes int layoutRes,
                @DimenRes int widthDimenRes,
                @DimenRes int heightDimenRes,
                @DimenRes int buttonWidthRes,
                @DimenRes int buttonMarginRes) {
            Resources res = context.getResources();
            layout = layoutRes;
            widgetWidthDp = getElementSizeInDP(res, widthDimenRes, 0);
            widgetHeightDp = getElementSizeInDP(res, heightDimenRes, 0);
            buttonWidthDp = getElementSizeInDP(res, buttonWidthRes, buttonMarginRes);
        }

        /**
         * Helper method to return width of an element id DP.
         *
         * @param res Resources.
         * @param mainDimenRes Core dimension resource id.
         * @param marginDimenRes Margin dimension resource id (optional, may be 0).
         * @return Element size measured in DP.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        static int getElementSizeInDP(
                Resources res, @DimenRes int mainDimenRes, @DimenRes int marginDimenRes) {
            if (mainDimenRes == 0) return 0;

            float density = res.getDisplayMetrics().density;
            float size = res.getDimension(mainDimenRes);
            if (marginDimenRes != 0) {
                size += 2 * res.getDimension(marginDimenRes);
            }
            return (int) (size / density);
        }

        /**
         * Given width of a target area compute how many buttons need to be hidden so that the
         * widget fits in the area without truncation. When there is not enough width, we compensate
         * by removing buttons.
         *
         * <p>The method makes zero assumptions about the number of buttons, ie. it may return
         * larger number than the total number of displayed buttons.
         *
         * @param areaWidthDp Width of the area where the widget is installed, expressed in dp.
         * @return Number of buttons that have to be hidden so that this widget fits correctly in
         *     the target area.
         */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        int computeNumberOfButtonsToHide(int areaWidthDp) {
            // We compute the number of buttons to hide by subtracting the area width from
            // the reference width (to check how much less space we have at our disposal), and
            // dividing the remaining number by the button width, rounding up to the nearest
            // integer. Negative values indicate we have "more space" than we need, hence no need to
            // remove any buttons, so we just ignore this.
            return (int)
                    Math.max(0, Math.ceil(1.0 * (widgetWidthDp - areaWidthDp) / buttonWidthDp));
        }
    }

    /** Class describing the widget button offerings. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static class WidgetButtonSettings {
        /** Whether Voice Search button should be visible. */
        public boolean voiceSearchVisible;

        /** Whether Incognito mode button should be visible. */
        public boolean incognitoModeVisible;

        /** Whether Google Lens button should be visible. */
        public boolean googleLensVisible;

        /** Whether Dino Game button should be visible. */
        public boolean dinoGameVisible;

        /** Default constructor, accessible only for tests. */
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        WidgetButtonSettings() {}

        /** Construct an instance of this class from the SearchActivityPreferences. */
        public WidgetButtonSettings(SearchActivityPreferences prefs) {
            voiceSearchVisible = prefs.voiceSearchAvailable;
            incognitoModeVisible = prefs.incognitoAvailable;
            googleLensVisible = prefs.googleLensAvailable;
            dinoGameVisible = true;
        }

        /**
         * Ensure that at least numButtonsToHide buttons are not visible.
         *
         * @param numButtonsToHide Number of buttons that should not be visible.
         */
        public void hideButtons(int numButtonsToHide) {
            // Compute the actual number of visible buttons.
            int numButtonsHidden =
                    (voiceSearchVisible ? 0 : 1)
                            + (incognitoModeVisible ? 0 : 1)
                            + (googleLensVisible ? 0 : 1)
                            + (dinoGameVisible ? 0 : 1);

            // The series of calls below determine the priority in which we decide to hide buttons.
            // In the event we run out of space, we hide Dino game first, then Lens and so on.
            if (numButtonsToHide > numButtonsHidden && dinoGameVisible) {
                dinoGameVisible = false;
                numButtonsHidden++;
            }

            if (numButtonsToHide > numButtonsHidden && googleLensVisible) {
                googleLensVisible = false;
                numButtonsHidden++;
            }

            if (numButtonsToHide > numButtonsHidden && incognitoModeVisible) {
                incognitoModeVisible = false;
                numButtonsHidden++;
            }

            if (numButtonsToHide > numButtonsHidden && voiceSearchVisible) {
                voiceSearchVisible = false;
                numButtonsHidden++;
            }
        }
    }

    /** The intent to create a new incognito tab. */
    private final @NonNull Intent mStartIncognitoTabIntent;

    /** The intent to begin the Dino game. */
    private final @NonNull Intent mStartDinoGameIntent;

    /** Widget variant describing the Medium widget. */
    private final @NonNull WidgetVariant mMediumWidgetVariant;

    /** Widget variant describing the Small widget. */
    private final @NonNull WidgetVariant mSmallWidgetVariant;

    /** Widget variant describing the Extra Small widget. */
    private final @NonNull WidgetVariant mExtraSmallWidgetVariant;

    /** Widget variant describing the Dino widget. */
    private final @NonNull WidgetVariant mDinoWidgetVariant;

    /**
     * @param context Context that can be used to pre-compute values. Do not cache.
     * @param searchActivityComponent Component linking to SearchActivity where all Search related
     *     events will be propagated.
     * @param startIncognitoTabIntent A trusted intent starting a new Incognito tab.
     * @param startDinoGameIntent A trusted intent starting the Dino game.
     */
    public QuickActionSearchWidgetProviderDelegate(
            @NonNull Context context,
            @NonNull Intent startIncognitoTabIntent,
            @NonNull Intent startDinoGameIntent) {
        mStartIncognitoTabIntent = startIncognitoTabIntent;
        mStartDinoGameIntent = startDinoGameIntent;

        context = context.getApplicationContext();
        mMediumWidgetVariant =
                new WidgetVariant(
                        context,
                        R.layout.quick_action_search_widget_medium_layout,
                        R.dimen.quick_action_search_widget_medium_width,
                        R.dimen.quick_action_search_widget_medium_height,
                        R.dimen.quick_action_search_widget_medium_button_width,
                        R.dimen.quick_action_search_widget_medium_button_horizontal_margin);

        mSmallWidgetVariant =
                new WidgetVariant(
                        context,
                        R.layout.quick_action_search_widget_small_layout,
                        R.dimen.quick_action_search_widget_small_width,
                        R.dimen.quick_action_search_widget_small_height,
                        R.dimen.quick_action_search_widget_small_button_width,
                        R.dimen.quick_action_search_widget_small_button_horizontal_margin);

        mExtraSmallWidgetVariant =
                new WidgetVariant(
                        context,
                        R.layout.quick_action_search_widget_xsmall_layout,
                        R.dimen.quick_action_search_widget_xsmall_width,
                        R.dimen.quick_action_search_widget_xsmall_height,
                        R.dimen.quick_action_search_widget_xsmall_button_width,
                        R.dimen.quick_action_search_widget_xsmall_button_horizontal_margin);
        mDinoWidgetVariant =
                new WidgetVariant(
                        context,
                        R.layout.quick_action_search_widget_dino_layout,
                        R.dimen.quick_action_search_widget_dino_size,
                        R.dimen.quick_action_search_widget_dino_size,
                        0,
                        0);
    }

    /**
     * Adjust button visibility to match feature availability and widget area.
     *
     * <p>This method shows/hides widget buttons in order to reflect feature availability, eg. any
     * feature that is not accessible to the user will have its corresponding button removed.
     *
     * <p>The method evaluates widget area width to determine if any additional buttons have to be
     * hidden in order to prevent button truncation: given widget variant and launcher area width
     * decide how many buttons have to be hidden in order to prevent the truncation and hide any
     * additional buttons in this order:
     *
     * <ul>
     *   <li>Dino Game (first),
     *   <li>Google Lens,
     *   <li>Incognito Mode,
     *   <li>Voice Search (last).
     * </ul>
     *
     * @param views RemoteViews structure that hosts the buttons.
     * @param prefs SearchActivityPreferences structure describing feature availability.
     * @param variant Target widget variant.
     * @param targetWidthDp The width of the space for the widget, as offered by the Launcher.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void applyRemoteViewsButtonVisibilityToFitWidth(
            @NonNull RemoteViews views,
            @NonNull SearchActivityPreferences prefs,
            @NonNull WidgetVariant variant,
            int targetWidthDp) {
        WidgetButtonSettings settings = new WidgetButtonSettings(prefs);
        settings.hideButtons(variant.computeNumberOfButtonsToHide(targetWidthDp));

        views.setViewVisibility(
                R.id.voice_search_quick_action_button,
                settings.voiceSearchVisible ? View.VISIBLE : View.GONE);
        views.setViewVisibility(
                R.id.incognito_quick_action_button,
                settings.incognitoModeVisible ? View.VISIBLE : View.GONE);
        views.setViewVisibility(
                R.id.lens_quick_action_button,
                settings.googleLensVisible ? View.VISIBLE : View.GONE);
        views.setViewVisibility(
                R.id.dino_quick_action_button, settings.dinoGameVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Given the width and height of the widget cell area (expressed in distance points) and the
     * screen density, compute vertical and horizontal paddings that have to be applied to make the
     * widget retain the square aspect ratio (1:1). The returned size is expressed in pixels.
     *
     * @param cellAreaWidthDp Width of the cell area, expressed in distance points.
     * @param cellAreaHeightDp Height of the cell area, expressed in distance points.
     * @param density Screen density.
     * @return Size object, describing required horizontal and vertical padding, expressed in
     *     pixels.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    Size computeWidgetAreaPaddingForDinoWidgetPx(
            int cellAreaWidthDp, int cellAreaHeightDp, float density) {
        int edgeLengthDp = Math.min(cellAreaWidthDp, cellAreaHeightDp);
        int width = (int) (((cellAreaWidthDp - edgeLengthDp) / 2.f) * density);
        int height = (int) (((cellAreaHeightDp - edgeLengthDp) / 2.f) * density);
        return new Size((int) width, (int) height);
    }

    /**
     * Given the width and height of the cell area (expressed in distance points) compute the scale
     * factor (expressed as a float value) that needs to be applied to relevant dimensions to scale
     * the widget proportionally.
     *
     * @param cellAreaWidthDp Width of the cell area, expressed in distance points.
     * @param cellAreaHeightDp Height of the cell area, expressed in distance points.
     * @param density Screen density.
     * @return Scale factor that should be applied to relevant dimensions to resize the widget
     *     proportionately.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    float computeScaleFactorForDinoWidget(int cellAreaWidthDp, int cellAreaHeightDp) {
        // Compute the paddings to better visually arrange the views inside the widget.
        // First, compute the scale factor. The scale factor is based on the reference dimensions
        // (ie. dimensions from the UX mocks) vs the on-screen area size (which almost certainly
        // will be different than the reference).
        // The scale factor reflects how much larger (or smaller) the cell area size is compared
        // to the mocks, ie. scale == 1.2 means that edgeSize is 20% larger than the reference size.
        final int edgeSize = Math.min(cellAreaWidthDp, cellAreaHeightDp);
        return 1.f * edgeSize / mDinoWidgetVariant.widgetWidthDp;
    }

    /**
     * @param resources Current resources.
     * @return Whether widget layout direction is RTL.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean isLayoutDirectionRTL(@NonNull Resources resources) {
        return resources.getConfiguration().getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void resizeDinoWidgetToFillTargetCellArea(
            @NonNull Resources resources, RemoteViews views, int areaWidthDp, int areaHeightDp) {
        float density = resources.getDisplayMetrics().density;

        // Screen density is used to compute padding in each direction.
        // The left/right and top/bottom dimensions are the same, since we want to center the view
        // in the area where we have some non-zero padding.
        Size paddings = computeWidgetAreaPaddingForDinoWidgetPx(areaWidthDp, areaHeightDp, density);
        views.setViewPadding(
                R.id.dino_quick_action_area,
                paddings.getWidth(),
                paddings.getHeight(),
                paddings.getWidth(),
                paddings.getHeight());

        // Now use the scale factor to modify all the dimensions that count.
        // We depend on the intrinsic view computations to infer a lot of these dimensions; the ones
        // below have to be computed by us.
        //
        // The core dimensions that have to be scaled by us are the paddings around the core widget
        // area (where the image is hosted), to make sure the content is properly adjusted.
        final float scale = computeScaleFactorForDinoWidget(areaWidthDp, areaHeightDp);
        final float contentPaddingVertical =
                resources.getDimension(R.dimen.quick_action_search_widget_dino_padding_vertical)
                        * scale;
        final float contentPaddingStart =
                resources.getDimension(R.dimen.quick_action_search_widget_dino_padding_start)
                        * scale;

        // Note: there is no setViewRelativePadding method available for RemoteViews. This means
        // we have to be smart about what edge means "start".
        final float contentPaddingLeft = isLayoutDirectionRTL(resources) ? 0 : contentPaddingStart;
        final float contentPaddingRight = isLayoutDirectionRTL(resources) ? contentPaddingStart : 0;

        views.setViewPadding(
                R.id.dino_quick_action_button,
                (int) contentPaddingLeft,
                (int) contentPaddingVertical,
                (int) contentPaddingRight,
                (int) contentPaddingVertical);

        // Scale text proportionately, ignoring the system font scaling. We have to apply this to
        // avoid cases where the system font setting leads to either:
        // - text truncation,
        // - overlapping the dino image with text, or
        // - making the text so small that it leaves a lot of empty space on the widget.
        final float textSize =
                resources.getDimension(R.dimen.quick_action_search_widget_dino_text_size)
                        * scale
                        / resources.getDisplayMetrics().scaledDensity;
        views.setFloat(R.id.dino_quick_action_text, "setTextSize", textSize);
    }

    /**
     * Create {@link RemoteViews} for the Dino widget.
     *
     * @param context Current context.
     * @param client SearchActivity client to use to talk to Search Activity.
     * @param prefs Structure describing current preferences and feature availability.
     * @param areaWidthDp Width of the widget area.
     * @param areaHeightDp Height of the widget area.
     * @return RemoteViews to be installed on the Dino widget.
     */
    public @NonNull RemoteViews createDinoWidgetRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityClient client,
            @NonNull SearchActivityPreferences prefs,
            int areaWidthDp,
            int areaHeightDp) {
        RemoteViews views =
                createWidgetRemoteViews(
                        context, client, R.layout.quick_action_search_widget_dino_layout);

        // Dino widget is specific; we want to scale up a lot of dimensions based on the actual size
        // of the widget area. This makes layout a lot more responsive but also a lot more
        // complicated since we have to compute everything manually.
        resizeDinoWidgetToFillTargetCellArea(
                context.getApplicationContext().getResources(), views, areaWidthDp, areaHeightDp);
        return views;
    }

    /**
     * Create configuration aware {@link RemoteViews} for the Search widget.
     *
     * <p>The returned RemoteViews are adjusted to fit given space, and respond to screen
     * orientation changes.
     *
     * @param context Current context.
     * @param client SearchActivity client to use to talk to Search Activity.
     * @param prefs Structure describing current preferences and feature availability.
     * @param areaWidthDp Width of the widget area.
     * @param areaHeightDp Height of the widget area.
     * @return RemoteViews to be installed on the Search widget for the passed variant.
     */
    public @NonNull RemoteViews createSearchWidgetRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityClient client,
            @NonNull SearchActivityPreferences prefs,
            int areaWidthDp,
            int areaHeightDp) {
        WidgetVariant variant = getSearchWidgetVariantForHeight(areaHeightDp);
        RemoteViews views = createWidgetRemoteViews(context, client, variant.layout);
        applyRemoteViewsButtonVisibilityToFitWidth(views, prefs, variant, areaWidthDp);
        return views;
    }

    /**
     * Given height, identify the layout that will fit in the space.
     *
     * @param context Current context.
     * @param heightDp Are height in distance points.
     * @return Widget LayoutRes appropriate for the supplied height.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    WidgetVariant getSearchWidgetVariantForHeight(int heightDp) {
        if (heightDp < mSmallWidgetVariant.widgetHeightDp) {
            return mExtraSmallWidgetVariant;
        } else if (heightDp < mMediumWidgetVariant.widgetHeightDp) {
            return mSmallWidgetVariant;
        }
        return mMediumWidgetVariant;
    }

    /**
     * Create a {@link RemoteViews} from supplied layoutRes. In this function, the appropriate
     * {@link PendingIntent} is assigned to each tap target on the widget.
     *
     * @param context The {@link Context} from which the widget is being updated.
     * @param client SearchActivity client to use to talk to Search Activity.
     * @param layoutRes The Layout to inflate.
     * @return Widget RemoteViews structure describing layout and content of the widget.
     */
    public RemoteViews createWidgetRemoteViews(
            @NonNull Context context,
            @NonNull SearchActivityClient client,
            @LayoutRes int layoutRes) {
        RemoteViews remoteViews = new RemoteViews(context.getPackageName(), layoutRes);

        // Search Bar Intent
        PendingIntent textSearchPendingIntent =
                createPendingIntentForAction(context, client, SearchType.TEXT);
        remoteViews.setOnClickPendingIntent(
                R.id.quick_action_search_widget_search_bar_container, textSearchPendingIntent);

        // Voice Search Intent
        PendingIntent voiceSearchPendingIntent =
                createPendingIntentForAction(context, client, SearchType.VOICE);
        remoteViews.setOnClickPendingIntent(
                R.id.voice_search_quick_action_button, voiceSearchPendingIntent);

        // Incognito Tab Intent
        PendingIntent incognitoTabPendingIntent =
                createPendingIntent(context, mStartIncognitoTabIntent);
        remoteViews.setOnClickPendingIntent(
                R.id.incognito_quick_action_button, incognitoTabPendingIntent);

        // Lens Search Intent
        PendingIntent lensSearchPendingIntent =
                createPendingIntentForAction(context, client, SearchType.LENS);
        remoteViews.setOnClickPendingIntent(R.id.lens_quick_action_button, lensSearchPendingIntent);

        // Dino Game intent
        PendingIntent dinoGamePendingIntent = createPendingIntent(context, mStartDinoGameIntent);
        remoteViews.setOnClickPendingIntent(R.id.dino_quick_action_button, dinoGamePendingIntent);

        return remoteViews;
    }

    /**
     * Creates a {@link PendingIntent} that will send a trusted intent with a specified action.
     *
     * @param context The Context from which the PendingIntent will perform the broadcast.
     * @param client SearchActivity client to use to talk to Search Activity.
     * @param action A String specifying the action for the intent.
     * @return A {@link PendingIntent} that will broadcast a trusted intent for the specified
     *     action.
     */
    private PendingIntent createPendingIntentForAction(
            @NonNull Context context,
            @NonNull SearchActivityClient client,
            @SearchType int searchType) {
        Intent intent =
                client.createIntent(
                        context, IntentOrigin.QUICK_ACTION_SEARCH_WIDGET, null, searchType);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return createPendingIntent(context, intent);
    }

    /**
     * Creates a {@link PendingIntent} that will send a trusted intent with a specified action.
     *
     * @param context The Context from which the PendingIntent will perform the broadcast.
     * @param intent An intent to execute.
     * @return A {@link PendingIntent} that will broadcast a trusted intent for the specified
     *     action.
     */
    private PendingIntent createPendingIntent(@NonNull Context context, @NonNull Intent intent) {
        return PendingIntent.getActivity(
                context,
                /* requestCode= */ 0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /** Returns the Medium widget variant for testing purposes. */
    WidgetVariant getMediumWidgetVariantForTesting() {
        return mMediumWidgetVariant;
    }

    /** Returns the Small widget variant for testing purposes. */
    WidgetVariant getSmallWidgetVariantForTesting() {
        return mSmallWidgetVariant;
    }

    /** Returns the Extra-small widget variant for testing purposes. */
    WidgetVariant getExtraSmallWidgetVariantForTesting() {
        return mExtraSmallWidgetVariant;
    }
}

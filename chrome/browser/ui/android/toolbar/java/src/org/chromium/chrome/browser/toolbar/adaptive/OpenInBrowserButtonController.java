// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.util.function.Supplier;

/**
 * Optional toolbar button which opens the current Custom Tab in BrApp. May be used by {@link
 * AdaptiveToolbarButtonController}.
 */
@NullMarked
public class OpenInBrowserButtonController extends BaseButtonDataProvider {

    private final Runnable mOpenInBrowserRunnable;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;

    /**
     * Creates {@code OpenInBrowserButtonController}.
     *
     * @param context The Context for retrieving resources, etc.
     * @param buttonDrawable Drawable for the new tab button.
     * @param activeTabSupplier Used to access the current tab.
     * @param openInBrowserRunnable Runnable opening the tab in browser.
     * @param trackerSupplier Supplier for the current profile tracker.
     */
    public OpenInBrowserButtonController(
            Context context,
            Drawable buttonDrawable,
            Supplier<@Nullable Tab> activeTabSupplier,
            Runnable openInBrowserRunnable,
            Supplier<@Nullable Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                context.getString(R.string.menu_open_in_product_default),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.OPEN_IN_BROWSER,
                /* tooltipTextResId= */ R.string.menu_open_in_product_default);
        setShouldShowOnIncognitoTabs(true);
        mOpenInBrowserRunnable = openInBrowserRunnable;
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    public void onClick(View view) {
        mOpenInBrowserRunnable.run();
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            String event = EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_OPEN_IN_BROWSER_OPENED;
            tracker.notifyEvent(event);
        }
        RecordUserAction.record("MobileTopToolbarOpenInBrowserButton");
    }

    /**
     * Returns an IPH for this button. Only called once native is initialized and when {@code
     * AdaptiveToolbarFeatures.isCustomizationEnabled()} is true.
     *
     * @param tab Current tab.
     */
    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        String feature =
                FeatureConstants
                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_OPEN_IN_BROWSER_FEATURE;
        IphCommandBuilder iphCommandBuilder =
                new IphCommandBuilder(
                                tab.getContext().getResources(),
                                feature,
                                R.string.adaptive_toolbar_button_open_in_browser_iph,
                                R.string.adaptive_toolbar_button_open_in_browser_iph)
                        .setHighlightParams(params);
        return iphCommandBuilder;
    }
}

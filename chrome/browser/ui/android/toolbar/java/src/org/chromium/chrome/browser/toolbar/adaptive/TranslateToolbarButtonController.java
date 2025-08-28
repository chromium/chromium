// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.util.function.Supplier;

/** Handles the translate button on the toolbar. */
@NullMarked
public class TranslateToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<@Nullable Tracker> mTrackerSupplier;

    /**
     * Creates a new instance of {@code TranslateButtonController}.
     *
     * @param activeTabSupplier Supplier for the current active tab.
     * @param buttonDrawable Drawable for the button icon.
     * @param contentDescription String for the button's content description.
     * @param trackerSupplier  Supplier for the current profile tracker, used for IPH.
     */
    public TranslateToolbarButtonController(
            Supplier<@Nullable Tab> activeTabSupplier,
            Drawable buttonDrawable,
            String contentDescription,
            Supplier<@Nullable Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                contentDescription,
                Resources.ID_NULL,
                /* supportsTinting= */ true,
                null,
                AdaptiveToolbarButtonVariant.TRANSLATE,
                /* tooltipTextResId= */ Resources.ID_NULL);
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IphCommandBuilder(
                tab.getContext().getResources(),
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_TRANSLATE_FEATURE,
                /* stringId= */ R.string.adaptive_toolbar_button_translate_iph,
                /* accessibilityStringId= */ R.string.adaptive_toolbar_button_translate_iph);
    }

    @Override
    public void onClick(View view) {
        Tab tab = mActiveTabSupplier.get();
        if (tab == null) return;

        RecordUserAction.record("MobileTopToolbarTranslateButton");
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_TRANSLATE_OPENED);
        }

        TranslateBridge.translateTabWhenReady(tab);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (tab == null) return false;
        if (!super.shouldShowButton(tab)) return false;
        if (tab.isNativePage() && assumeNonNull(tab.getNativePage()).isPdf()) return false;
        return UrlUtilities.isHttpOrHttps(tab.getUrl());
    }
}

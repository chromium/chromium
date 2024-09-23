// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Handles the translate button on the toolbar. */
public class TranslateToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<Tracker> mTrackerSupplier;

    /**
     * Creates a new instance of {@code TranslateButtonController}.
     *
     * @param activeTabSupplier Supplier for the current active tab.
     * @param buttonDrawable Drawable for the button icon.
     * @param contentDescription String for the button's content description.
     * @param trackerSupplier  Supplier for the current profile tracker, used for IPH.
     */
    public TranslateToolbarButtonController(
            Supplier<Tab> activeTabSupplier,
            Drawable buttonDrawable,
            String contentDescription,
            Supplier<Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                contentDescription,
                Resources.ID_NULL,
                /* supportsTinting= */ true,
                null,
                AdaptiveToolbarButtonVariant.TRANSLATE,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ true);
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IPHCommandBuilder(
                tab.getContext().getResources(),
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_TRANSLATE_FEATURE,
                /* stringId= */ R.string.adaptive_toolbar_button_translate_iph,
                /* accessibilityStringId= */ R.string.adaptive_toolbar_button_translate_iph);
    }

    @Override
    public void onClick(View view) {
        if (!mActiveTabSupplier.hasValue()) return;

        RecordUserAction.record("MobileTopToolbarTranslateButton");
        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_TRANSLATE_OPENED);
        }

        TranslateBridge.translateTabWhenReady(mActiveTabSupplier.get());
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (!super.shouldShowButton(tab)) return false;
        if (tab.isNativePage() && tab.getNativePage().isPdf()) return false;
        return UrlUtilities.isHttpOrHttps(tab.getUrl());
    }
}

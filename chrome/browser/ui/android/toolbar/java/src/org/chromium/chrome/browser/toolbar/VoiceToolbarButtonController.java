// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Handles displaying the voice search button on toolbar depending on several conditions (e.g.
 * device width, whether NTP is shown, whether voice is enabled).
 *
 * <p>TODO(crbug.com/40729195): Move this to ../voice/ along with VoiceRecognitionHandler and the
 * assistant support.
 */
public class VoiceToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<Tracker> mTrackerSupplier;

    private final VoiceSearchDelegate mVoiceSearchDelegate;

    /** Delegate interface for interacting with voice search. */
    public interface VoiceSearchDelegate {
        /**
         * @return True if voice search is enabled for the current session.
         */
        boolean isVoiceSearchEnabled();

        /** Starts a voice search interaction. */
        void startVoiceRecognition();
    }

    /**
     * Creates a VoiceToolbarButtonController object.
     *
     * @param context The context for retrieving string resources.
     * @param buttonDrawable Drawable for the voice button.
     * @param activeTabSupplier Provides the currently displayed {@link Tab}.
     * @param trackerSupplier  Supplier for the current profile tracker.
     * @param modalDialogManager Dispatcher for modal lifecycle events
     * @param voiceSearchDelegate Provides interaction with voice search.
     */
    public VoiceToolbarButtonController(
            Context context,
            Drawable buttonDrawable,
            Supplier<Tab> activeTabSupplier,
            Supplier<Tracker> trackerSupplier,
            ModalDialogManager modalDialogManager,
            VoiceSearchDelegate voiceSearchDelegate) {
        super(
                activeTabSupplier,
                modalDialogManager,
                buttonDrawable,
                context.getString(R.string.accessibility_toolbar_btn_mic),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.VOICE,
                /* tooltipTextResId= */ R.string.adaptive_toolbar_button_preference_voice_search,
                /* showHoverHighlight= */ true);
        mTrackerSupplier = trackerSupplier;
        mVoiceSearchDelegate = voiceSearchDelegate;
    }

    @Override
    public void onClick(View view) {
        RecordUserAction.record("MobileTopToolbarVoiceButton");
        mVoiceSearchDelegate.startVoiceRecognition();

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_OPENED);
        }
    }

    /** Triggers checking and possibly updating the mic visibility */
    public void updateMicButtonState() {
        mButtonData.setCanShow(shouldShowButton(mActiveTabSupplier.get()));
        notifyObservers(mButtonData.canShow());
    }

    /**
     * Returns an IPH for this button. Only called once native is initialized and when {@code
     * AdaptiveToolbarFeatures.isCustomizationEnabled()} is true.
     * @param tab Current tab.
     */
    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                                tab.getContext().getResources(),
                                FeatureConstants
                                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE,
                                /* stringId= */ R.string.adaptive_toolbar_button_voice_search_iph,
                                /* accessibilityStringId= */ R.string
                                        .adaptive_toolbar_button_voice_search_iph)
                        .setHighlightParams(params);

        return iphCommandBuilder;
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (!super.shouldShowButton(tab)) return false;

        return mVoiceSearchDelegate.isVoiceSearchEnabled()
                && UrlUtilities.isHttpOrHttps(tab.getUrl());
    }

    /** Returns whether the feature flags allow showing the mic icon in the toolbar. */
    public static boolean isToolbarMicEnabled() {
        if (!FeatureList.isInitialized()) return false;
        return AdaptiveToolbarFeatures.isCustomizationEnabled();
    }
}

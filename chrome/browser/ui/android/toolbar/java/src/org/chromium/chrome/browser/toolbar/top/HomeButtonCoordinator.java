// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.FeatureParamUtils;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

/**
 * Root component for the home button on the toolbar. Intended to own the {@link HomeButton}, but
 * currently it only manages some signals around the home page button.
 * TODO(https://crbug.com/871806): Finish converting HomeButton to MVC and move more logic into this
 * class.
 */
public class HomeButtonCoordinator {
    @VisibleForTesting
    static final String MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME = "isMainIntentFromLauncher";
    @VisibleForTesting
    static final String INTENT_WITH_EFFECT_PARAM_NAME = "intentWithEffect";

    private final Context mContext;
    private final View mHomeButton;
    private final BooleanSupplier mIsFeedEnabled;
    private final UserEducationHelper mUserEducationHelper;
    private final BooleanSupplier mIsIncognitoSupplier;
    private final CurrentTabObserver mPageLoadObserver;
    private final OneshotSupplier<ToolbarIntentMetadata> mIntentMetadataOneshotSupplier;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final Supplier<Boolean> mIsHomepageNonNtpSupplier;

    /**
     * @param context The Android context used for various view operations.
     * @param homeButton The concrete {@link View} class for this MVC component.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param isIncognitoSupplier Supplier for whether the current tab is incognito.
     * @param intentMetadataOneshotSupplier Potentially delayed information about launching intent.
     * @param promoShownOneshotSupplier Potentially delayed information about if a promo was shown.
     * @param isHomepageNonNtpSupplier Supplier for whether the current homepage is not NTP.
     * @param isFeedEnabled Supplier for whether feed is enabled.
     * @param tabSupplier Supplier of the activity tab.
     */
    public HomeButtonCoordinator(@NonNull Context context, @Nullable View homeButton,
            @NonNull UserEducationHelper userEducationHelper,
            @NonNull BooleanSupplier isIncognitoSupplier,
            @NonNull OneshotSupplier<ToolbarIntentMetadata> intentMetadataOneshotSupplier,
            @NonNull OneshotSupplier<Boolean> promoShownOneshotSupplier,
            @NonNull Supplier<Boolean> isHomepageNonNtpSupplier,
            @NonNull BooleanSupplier isFeedEnabled, @NonNull ObservableSupplier<Tab> tabSupplier) {
        mContext = context;
        mHomeButton = homeButton;
        mUserEducationHelper = userEducationHelper;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mIntentMetadataOneshotSupplier = intentMetadataOneshotSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mIsHomepageNonNtpSupplier = isHomepageNonNtpSupplier;
        mIsFeedEnabled = isFeedEnabled;
        mPageLoadObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // Part of scroll jank investigation http://crbug.com/1311003. Will remove
                // TraceEvent after the investigation is complete.
                try (TraceEvent te =
                                TraceEvent.scoped("HomeButtonCoordinator::onPageLoadFinished")) {
                    handlePageLoadFinished(url);
                }
            }
        }, /*swapCallback=*/null);
    }

    /** Cleans up observers. */
    public void destroy() {
        mPageLoadObserver.destroy();
    }

    /**
     * TODO(https://crbug.com/1133355): Reduce visibility once ActivityTabTabObserver is mockable.
     * @param url The URL of the current page that was just loaded.
     */
    @VisibleForTesting
    void handlePageLoadFinished(GURL url) {
        if (mHomeButton == null || !mHomeButton.isShown()) return;
        if (mIsIncognitoSupplier.getAsBoolean()) return;
        if (UrlUtilities.isNTPUrl(url)) return;
        if (mIsHomepageNonNtpSupplier.get()) return;
        if (mPromoShownOneshotSupplier.get() == null || mPromoShownOneshotSupplier.get()) return;

        ToolbarIntentMetadata intentMetadata = mIntentMetadataOneshotSupplier.get();
        if (intentMetadata == null) return;
        if (FeatureParamUtils.paramExistsAndDoesNotMatch(
                    FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
                    MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME,
                    intentMetadata.getIsMainIntentFromLauncher())) {
            return;
        }
        if (FeatureParamUtils.paramExistsAndDoesNotMatch(
                    FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
                    INTENT_WITH_EFFECT_PARAM_NAME, intentMetadata.getIsIntentWithEffect())) {
            return;
        }

        boolean hasFeed = mIsFeedEnabled.getAsBoolean();
        int textId = hasFeed ? R.string.iph_ntp_with_feed_text : R.string.iph_ntp_without_feed_text;
        int accessibilityTextId = hasFeed ? R.string.iph_ntp_with_feed_accessibility_text
                                          : R.string.iph_ntp_without_feed_accessibility_text;

        mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(mContext.getResources(),
                FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE, textId, accessibilityTextId)
                                                    .setAnchorView(mHomeButton)
                                                    .setHighlightParams(new HighlightParams(
                                                            HighlightShape.CIRCLE))
                                                    .build());
    }
}

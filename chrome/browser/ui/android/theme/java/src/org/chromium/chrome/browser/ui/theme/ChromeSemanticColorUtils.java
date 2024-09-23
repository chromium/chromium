// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.theme;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.theme.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Chrome specific version of {@link SemanticColorUtils}. */
public class ChromeSemanticColorUtils {
    /**
     * Returns the semantic color value that corresponds to
     * contextual_search_promo_background_color.
     */
    public static @ColorInt int getContextualSearchPromoBackgroundColor(Context context) {
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    /** Returns the semantic color value that corresponds to overlay_panel_bar_background_color. */
    public static @ColorInt int getOverlayPanelBarBackgroundColor(Context context) {
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    /** Returns the semantic color value that corresponds to payment_request_bg. */
    public static @ColorInt int getPaymentRequestBg(Context context) {
        return SemanticColorUtils.getSheetBgColor(context);
    }

    /** Returns the semantic color value that corresponds to offline_indicator_back_online_color. */
    public static @ColorInt int getOfflineIndicatorBackOnlineColor(Context context) {
        return SemanticColorUtils.getDefaultControlColorActive(context);
    }

    /** Returns the semantic color value that corresponds to tab_inactive_hover_color. */
    public static @ColorInt int getTabInactiveHoverColor(Context context) {
        return SemanticColorUtils.getDefaultControlColorActive(context);
    }

    /** Returns the semantic color value that corresponds to an IPH highlight. */
    public static @ColorInt int getIphHighlightColor(Context context) {
        return ColorUtils.setAlphaComponentWithFloat(
                SemanticColorUtils.getDefaultControlColorActive(context),
                ResourcesCompat.getFloat(context.getResources(), R.dimen.iph_highlight_alpha));
    }
}

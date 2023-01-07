// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.res.Resources;

import androidx.core.view.ViewCompat;
import androidx.core.widget.TextViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the header suggestion view. */
public class HeaderViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, HeaderView view, PropertyKey propertyKey) {
        if (HeaderViewProperties.TITLE == propertyKey) {
            view.getTextView().setText(model.get(HeaderViewProperties.TITLE));
        } else if (propertyKey == SuggestionCommonProperties.COLOR_SCHEME) {
            final boolean isIncognito = model.get(SuggestionCommonProperties.COLOR_SCHEME)
                    == BrandedColorScheme.INCOGNITO;
            TextViewCompat.setTextAppearance(
                    view.getTextView(), ChromeColors.getTextMediumThickSecondaryStyle(isIncognito));
            ApiCompatibilityUtils.setImageTintList(view.getIconView(),
                    ChromeColors.getPrimaryIconTint(view.getContext(), isIncognito));
        } else if (propertyKey == SuggestionCommonProperties.LAYOUT_DIRECTION) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (propertyKey == HeaderViewProperties.IS_COLLAPSED) {
            boolean isCollapsed = model.get(HeaderViewProperties.IS_COLLAPSED);
            view.getIconView().setImageResource(isCollapsed ? R.drawable.ic_expand_more_black_24dp
                                                            : R.drawable.ic_expand_less_black_24dp);
            view.setCollapsedStateForAccessibility(isCollapsed);
        } else if (propertyKey == HeaderViewProperties.DELEGATE) {
            HeaderViewProperties.Delegate delegate = model.get(HeaderViewProperties.DELEGATE);
            if (delegate != null) {
                view.setOnClickListener(v -> delegate.onHeaderClicked());
                view.setOnSelectListener(delegate::onHeaderSelected);
            } else {
                view.setOnClickListener(null);
                view.setOnSelectListener(null);
            }
        } else if (propertyKey == HeaderViewProperties.SHOULD_REMOVE_CHEVRON) {
            view.setShouldRemoveSuggestionHeaderChevron(
                    model.get(HeaderViewProperties.SHOULD_REMOVE_CHEVRON));
        } else if (propertyKey == HeaderViewProperties.SHOULD_REMOVE_CAPITALIZATION) {
            view.setShouldRemoveSuggestionHeaderCapitalization(
                    model.get(HeaderViewProperties.SHOULD_REMOVE_CAPITALIZATION));
        } else if (propertyKey == HeaderViewProperties.USE_UPDATED_HEADER_PADDING) {
            boolean useUpdatedHeaderPadding =
                    model.get(HeaderViewProperties.USE_UPDATED_HEADER_PADDING);
            Resources res = view.getResources();

            int minHeight = res.getDimensionPixelSize(useUpdatedHeaderPadding
                            ? R.dimen.omnibox_suggestion_header_height_modern
                            : R.dimen.omnibox_suggestion_header_height);

            int paddingStart = res.getDimensionPixelSize(useUpdatedHeaderPadding
                            ? R.dimen.omnibox_suggestion_header_padding_start_modern
                            : R.dimen.omnibox_suggestion_header_padding_start);

            int paddingTop = useUpdatedHeaderPadding
                    ? res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_top)
                    : 0;

            int paddingBottom = useUpdatedHeaderPadding
                    ? res.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_bottom)
                    : 0;

            // Use modified start padding if the phase 2 feature is enabled.
            if (OmniboxFeatures.shouldShowModernizeVisualUpdate(view.getContext())) {
                minHeight = res.getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_header_height_modern_phase2);
                paddingStart += res.getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing);
                // TODO(crbug.com/1372596): Header view is off center and we should fix this.
                paddingBottom = 0;
            }

            view.setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
        }
    }
}

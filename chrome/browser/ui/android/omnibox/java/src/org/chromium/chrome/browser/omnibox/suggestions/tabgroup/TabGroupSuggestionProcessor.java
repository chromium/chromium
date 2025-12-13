// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tabgroup;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.text.Spannable;
import android.text.style.ImageSpan;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that handles model and view creation for the tab group omnibox suggestion backed by a
 * {@link SavedTabGroup} object.
 */
@NullMarked
public class TabGroupSuggestionProcessor extends BaseSuggestionViewProcessor {
    /**
     * @param uiContext Context object containing common UI dependencies.
     */
    public TabGroupSuggestionProcessor(AutocompleteUIContext uiContext) {
        super(uiContext);
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.TAB_GROUP;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TAB_GROUP_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(
            AutocompleteInput input,
            AutocompleteMatch suggestion,
            PropertyModel model,
            int position) {
        super.populateModel(input, suggestion, model, position);

        OmniboxDrawableState state =
                new OmniboxDrawableState(
                        ContextCompat.getDrawable(mContext, R.drawable.ic_features_24dp),
                        /* useRoundedCorners= */ false,
                        /* isLarge= */ false,
                        /* allowTint= */ true);
        model.set(BaseSuggestionViewProperties.ICON, state);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, getTitleSpannable(suggestion));
        SuggestionSpannable descriptionText = new SuggestionSpannable(suggestion.getDescription());
        applyHighlightToMatchRegions(descriptionText, suggestion.getDescriptionClassifications());
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, descriptionText);
        model.set(SuggestionViewProperties.CONTENT_DESCRIPTION, getContentDescription(suggestion));
    }

    private SuggestionSpannable getTitleSpannable(AutocompleteMatch suggestion) {
        int imageSpanEdgeSize =
                (int)
                        mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.hub_search_tab_group_image_span_edge_size);
        int plainColorId = Integer.parseInt(assumeNonNull(suggestion.getImageDominantColor()));
        @TabGroupColorId
        int colorId = TabGroupColorPickerUtils.getTabGroupCardColorId(plainColorId);
        @ColorInt
        int color =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, colorId, /* isIncognito= */ false);
        ShapeDrawable inlineColorIcon = new ShapeDrawable(new OvalShape());
        inlineColorIcon.setBounds(
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ imageSpanEdgeSize,
                /* bottom= */ imageSpanEdgeSize);
        inlineColorIcon.getPaint().setColor(color);
        SuggestionSpannable titleSpannable =
                new SuggestionSpannable("   " + suggestion.getDisplayText());
        titleSpannable.setSpan(
                new ImageSpan(inlineColorIcon, ImageSpan.ALIGN_CENTER),
                /* start= */ 0,
                /* end= */ 1,
                /* flags= */ Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        return titleSpannable;
    }

    private String getContentDescription(AutocompleteMatch suggestion) {
        int plainColorId = Integer.parseInt(assumeNonNull(suggestion.getImageDominantColor()));
        @TabGroupColorId
        int colorId = TabGroupColorPickerUtils.getTabGroupCardColorId(plainColorId);
        @StringRes
        int colorDescRes =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        colorId);
        String colorDesc = mContext.getString(colorDescRes);
        return mContext.getString(
                R.string.accessibility_tab_group_suggestion_description,
                suggestion.getDisplayText(),
                colorDesc,
                suggestion.getDescription());
    }
}

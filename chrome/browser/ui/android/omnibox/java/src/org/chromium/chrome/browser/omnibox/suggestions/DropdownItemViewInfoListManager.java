// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Px;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.GroupsProto.GroupSection;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Manages the list of DropdownItemViewInfo elements. */
class DropdownItemViewInfoListManager {
    private final Context mContext;
    private final ModelList mManagedModel;
    private int mLayoutDirection;
    private @BrandedColorScheme int mBrandedColorScheme;
    private List<DropdownItemViewInfo> mSourceViewInfoList;

    private final int mListActiveOmniboxTopSmallMargin;
    private final int mListActiveOmniboxTopBigMargin;
    private final int mListNonActiveOmniboxTopSmallMargin;
    private final int mListNonActiveOmniboxTopBigMargin;

    DropdownItemViewInfoListManager(@NonNull ModelList managedModel, @NonNull Context context) {
        assert managedModel != null : "Must specify a non-null model.";
        mContext = context;
        mLayoutDirection = View.LAYOUT_DIRECTION_INHERIT;
        mBrandedColorScheme = BrandedColorScheme.LIGHT_BRANDED_THEME;
        mSourceViewInfoList = Collections.emptyList();
        mManagedModel = managedModel;

        mListActiveOmniboxTopSmallMargin =
                OmniboxResourceProvider.getActiveOmniboxTopSmallMargin(context);
        mListActiveOmniboxTopBigMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_active_top_big_margin);
        mListNonActiveOmniboxTopSmallMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_non_active_top_small_margin);
        mListNonActiveOmniboxTopBigMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_non_active_top_big_margin);
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        if (mLayoutDirection == layoutDirection) return;

        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
        }
    }

    /**
     * Specifies the visual theme to be used by the suggestions.
     * @param brandedColorScheme Specifies which {@link BrandedColorScheme} should be used.
     */
    void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        if (mBrandedColorScheme == brandedColorScheme) return;

        mBrandedColorScheme = brandedColorScheme;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.COLOR_SCHEME, brandedColorScheme);
        }
    }

    /** Clear all DropdownItemViewInfo lists. */
    void clear() {
        mSourceViewInfoList.clear();
        mManagedModel.clear();
    }

    void onNativeInitialized() {}

    /**
     * Specify the input list of DropdownItemViewInfo elements.
     *
     * @param sourceList Source list of ViewInfo elements.
     */
    void setSourceViewInfoList(@NonNull List<DropdownItemViewInfo> sourceList) {
        mSourceViewInfoList = sourceList;

        // Build a new list of suggestions. Honor the default collapsed state.
        final List<ListItem> suggestionsList = new ArrayList<>();
        int deviceType = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                ? SuggestionCommonProperties.FormFactor.TABLET
                : SuggestionCommonProperties.FormFactor.PHONE;
        DropdownItemViewInfo previousItem = null;
        boolean useSmallestMargins = OmniboxFeatures.shouldShowSmallestMargins(mContext);
        int groupTopMargin = OmniboxResourceProvider.getSuggestionGroupTopMargin(mContext);
        int groupBottomMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_smallest_margin);
        int suggestionVerticalMargin = useSmallestMargins
                ? 0
                : mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_vertical_margin);

        GroupSection previousSection = null;
        GroupSection currentSection;
        boolean shouldShowModernizeVisualUpdate =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext);
        boolean previousItemWasHeader = false;

        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            final DropdownItemViewInfo item = mSourceViewInfoList.get(i);
            final PropertyModel model = item.model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.COLOR_SCHEME, mBrandedColorScheme);
            model.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, deviceType);

            if (shouldShowModernizeVisualUpdate && item.processor.allowBackgroundRounding()) {
                currentSection = item.groupConfig.getSection();
                var applyRounding = currentSection != previousSection;
                int topMargin;
                if (previousItemWasHeader) {
                    topMargin = mContext.getResources().getDimensionPixelSize(
                            R.dimen.omnibox_suggestion_group_vertical_margin);
                } else {
                    topMargin = applyRounding ? groupTopMargin : suggestionVerticalMargin;
                }
                var bottomMargin = applyRounding ? groupBottomMargin : suggestionVerticalMargin;

                model.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, applyRounding);
                // Do not have margin for the first suggestion, otherwise the first suggestion will
                // have a big gap with the Omnibox.
                model.set(DropdownCommonProperties.TOP_MARGIN,
                        previousItem == null
                                ? getSuggestionListTopMargin(item.processor.getViewTypeId())
                                : topMargin);

                if (previousItem != null) {
                    previousItem.model.set(
                            DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, applyRounding);
                    previousItem.model.set(DropdownCommonProperties.BOTTOM_MARGIN, bottomMargin);
                    previousItem.model.set(DropdownCommonProperties.SHOW_DIVIDER, !applyRounding);
                }

                previousItem = item;
                previousSection = currentSection;
            }

            previousItemWasHeader = item.processor.getViewTypeId() == OmniboxSuggestionUiType.HEADER
                    && shouldShowModernizeVisualUpdate
                    && (useSmallestMargins || OmniboxFeatures.shouldShowSmallerMargins(mContext));

            suggestionsList.add(item);
        }

        // round the bottom corners of the last suggestion.
        if (previousItem != null) {
            previousItem.model.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, true);
        }

        mManagedModel.set(suggestionsList);
    }

    /**
     * Return the top margin for the suggestion list in pixel size.
     * The padding size between the Omnibox and the top suggestion is dependent on the top
     * suggestion type and variations of the experiment:
     * 1. If the type is EDIT_URL_SUGGESTION, being an integral part of the Search Ready Omnibox
     * appears closer to the Omnibox.
     * 2. Everything else is spaced farther away, keeping the same distance between the Omnibox and
     * the Top suggestion as the distance between individual Suggestion sections.
     *
     * @param firstSuggestionUiType The type of the first suggestion.
     */
    private @Px int getSuggestionListTopMargin(@OmniboxSuggestionUiType int firstSuggestionUiType) {
        if (firstSuggestionUiType == OmniboxSuggestionUiType.EDIT_URL_SUGGESTION) {
            return OmniboxFeatures.shouldShowSmallerMargins(mContext)
                    ? mListActiveOmniboxTopSmallMargin
                    : 0;
        }

        if (OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            if (OmniboxFeatures.shouldShowSmallestMargins(mContext)) {
                return 0;
            } else if (OmniboxFeatures.shouldShowSmallBottomMargin()
                    || OmniboxFeatures.shouldShowSmallerMargins(mContext)) {
                return mListActiveOmniboxTopSmallMargin;
            } else {
                return mListActiveOmniboxTopBigMargin;
            }
        } else {
            if (OmniboxFeatures.shouldShowSmallBottomMargin()) {
                return mListNonActiveOmniboxTopSmallMargin;
            } else {
                return mListNonActiveOmniboxTopBigMargin;
            }
        }
    }
}

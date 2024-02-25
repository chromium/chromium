// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.GroupsProto.GroupSection;
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
        mListActiveOmniboxTopBigMargin =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_active_top_big_margin);
        mListNonActiveOmniboxTopSmallMargin =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_non_active_top_small_margin);
        mListNonActiveOmniboxTopBigMargin =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.omnibox_suggestion_list_non_active_top_big_margin);
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     *
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
     *
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
        int deviceType =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                        ? SuggestionCommonProperties.FormFactor.TABLET
                        : SuggestionCommonProperties.FormFactor.PHONE;
        DropdownItemViewInfo previousItem = null;
        GroupSection previousSection = null;
        GroupSection currentSection;
        boolean shouldShowModernizeVisualUpdate =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext);

        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            final DropdownItemViewInfo item = mSourceViewInfoList.get(i);
            final PropertyModel model = item.model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.COLOR_SCHEME, mBrandedColorScheme);
            model.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, deviceType);

            if (shouldShowModernizeVisualUpdate && item.processor.allowBackgroundRounding()) {
                currentSection = item.groupConfig.getSection();
                var applyRounding = currentSection != previousSection;

                model.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, applyRounding);

                if (previousItem != null) {
                    previousItem.model.set(
                            DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, applyRounding);
                    previousItem.model.set(DropdownCommonProperties.SHOW_DIVIDER, !applyRounding);
                }

                previousItem = item;
                previousSection = currentSection;
            }

            suggestionsList.add(item);
        }

        // round the bottom corners of the last suggestion.
        if (previousItem != null) {
            previousItem.model.set(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, true);
        }

        mManagedModel.set(suggestionsList);
    }
}

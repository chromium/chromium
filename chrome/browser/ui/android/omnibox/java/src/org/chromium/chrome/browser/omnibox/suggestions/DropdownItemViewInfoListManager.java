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
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.GroupsProto.GroupSection;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
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
    private boolean mDropdownItemRoundingEnabled;

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

        mListActiveOmniboxTopSmallMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_active_top_small_margin);
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

    /** @return Whether the supplied view info is a header for the specific group of suggestions. */
    private boolean isGroupHeaderWithId(DropdownItemViewInfo info, int groupId) {
        return (info.type == OmniboxSuggestionUiType.HEADER && info.groupId == groupId);
    }

    /** Clear all DropdownItemViewInfo lists. */
    void clear() {
        mSourceViewInfoList.clear();
        mManagedModel.clear();
    }

    void onNativeInitialized() {
        mDropdownItemRoundingEnabled = OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext);
    }

    /**
     * Specify the input list of DropdownItemViewInfo elements.
     *
     * @param sourceList Source list of ViewInfo elements.
     * @param groupsDetails Group ID to GroupConfig map carrying group collapsed state information.
     */
    void setSourceViewInfoList(
            @NonNull List<DropdownItemViewInfo> sourceList, @NonNull GroupsInfo groupsInfo) {
        mSourceViewInfoList = sourceList;

        final var groupsDetails = groupsInfo.getGroupConfigsMap();

        // Build a new list of suggestions. Honor the default collapsed state.
        final List<ListItem> suggestionsList = new ArrayList<>();
        int deviceType = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                ? SuggestionCommonProperties.FormFactor.TABLET
                : SuggestionCommonProperties.FormFactor.PHONE;
        DropdownItemViewInfo previousItem = null;
        boolean inDropdownItemBackgroundRoundingGroup = false;
        int groupTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int groupBottomMargin = mContext.getResources().getDimensionPixelSize(
                OmniboxFeatures.shouldShowSmallBottomMargin()
                        ? R.dimen.omnibox_suggestion_group_vertical_small_bottom_margin
                        : R.dimen.omnibox_suggestion_group_vertical_margin);
        int suggestionVerticalMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        GroupSection previousSection = null;
        GroupSection currentSection;

        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            final DropdownItemViewInfo item = mSourceViewInfoList.get(i);
            final PropertyModel model = item.model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.COLOR_SCHEME, mBrandedColorScheme);
            model.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, deviceType);

            if (mDropdownItemRoundingEnabled && item.processor.allowBackgroundRounding()) {
                var groupConfig = groupsDetails.get(item.groupId);
                currentSection = groupConfig != null ? groupConfig.getSection()
                                                     : GroupSection.SECTION_DEFAULT;
                var applyRounding = currentSection != previousSection;
                var topMargin = applyRounding ? groupTopMargin : suggestionVerticalMargin;
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

    /**
     * Return if the suggestion type should have background.
     *
     * @param type The type of the suggestion.
     */
    private boolean suggestionShouldHaveBackground(@OmniboxSuggestionUiType int type) {
        return OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext)
                && (type == OmniboxSuggestionUiType.DEFAULT
                        || type == OmniboxSuggestionUiType.EDIT_URL_SUGGESTION
                        || type == OmniboxSuggestionUiType.ANSWER_SUGGESTION
                        || type == OmniboxSuggestionUiType.ENTITY_SUGGESTION
                        || type == OmniboxSuggestionUiType.TAIL_SUGGESTION
                        || type == OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION
                        || type == OmniboxSuggestionUiType.PEDAL_SUGGESTION);
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
        if (!mDropdownItemRoundingEnabled
                || firstSuggestionUiType == OmniboxSuggestionUiType.EDIT_URL_SUGGESTION) {
            return 0;
        }

        if (OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            if (OmniboxFeatures.shouldShowSmallBottomMargin()) {
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

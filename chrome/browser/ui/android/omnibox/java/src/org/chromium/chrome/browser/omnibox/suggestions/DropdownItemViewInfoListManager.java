// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteResult;
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
    private final SparseBooleanArray mGroupsCollapsedState;
    private int mLayoutDirection;
    private @BrandedColorScheme int mBrandedColorScheme;
    private List<DropdownItemViewInfo> mSourceViewInfoList;

    DropdownItemViewInfoListManager(@NonNull ModelList managedModel, @NonNull Context context) {
        assert managedModel != null : "Must specify a non-null model.";
        mContext = context;
        mLayoutDirection = View.LAYOUT_DIRECTION_INHERIT;
        mBrandedColorScheme = BrandedColorScheme.LIGHT_BRANDED_THEME;
        mSourceViewInfoList = Collections.emptyList();
        mGroupsCollapsedState = new SparseBooleanArray();
        mManagedModel = managedModel;
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

    /**
     * Toggle the collapsed state of suggestions belonging to specific group.
     *
     * @param groupId ID of the group whose collapsed state is expected to change.
     * @param groupIsCollapsed Collapsed state of the group.
     */
    void setGroupCollapsedState(int groupId, boolean groupIsCollapsed) {
        if (getGroupCollapsedState(groupId) == groupIsCollapsed) return;
        mGroupsCollapsedState.put(groupId, groupIsCollapsed);
        if (groupIsCollapsed) {
            removeSuggestionsForGroup(groupId);
        } else {
            insertSuggestionsForGroup(groupId);
        }
    }

    /**
     * @param groupId ID of the suggestions group.
     * @return True, if group should be collapsed, otherwise false.
     */
    private boolean getGroupCollapsedState(int groupId) {
        return mGroupsCollapsedState.get(groupId, /* defaultCollapsedState= */ false);
    }

    /** @return Whether the supplied view info is a header for the specific group of suggestions. */
    private boolean isGroupHeaderWithId(DropdownItemViewInfo info, int groupId) {
        return (info.type == OmniboxSuggestionUiType.HEADER && info.groupId == groupId);
    }

    /** Clear all DropdownItemViewInfo lists. */
    void clear() {
        mSourceViewInfoList.clear();
        mManagedModel.clear();
        mGroupsCollapsedState.clear();
    }

    /**
     * Specify the input list of DropdownItemViewInfo elements.
     *
     * @param sourceList Source list of ViewInfo elements.
     * @param groupsDetails Group ID to GroupDetails map carrying group collapsed state information.
     */
    void setSourceViewInfoList(@NonNull List<DropdownItemViewInfo> sourceList,
            @NonNull SparseArray<AutocompleteResult.GroupDetails> groupsDetails) {
        mSourceViewInfoList = sourceList;
        mGroupsCollapsedState.clear();

        // Clone information about the recommended group collapsed state.
        for (int index = 0; index < groupsDetails.size(); index++) {
            mGroupsCollapsedState.put(
                    groupsDetails.keyAt(index), groupsDetails.valueAt(index).collapsedByDefault);
        }

        // Build a new list of suggestions. Honor the default collapsed state.
        final List<ListItem> suggestionsList = new ArrayList<>();
        int deviceType = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                ? SuggestionCommonProperties.FormFactor.TABLET
                : SuggestionCommonProperties.FormFactor.PHONE;
        DropdownItemViewInfo prevSuggestionWithBackground = null;
        boolean inDropdownItemBackgroundRoundingGroup = false;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            final DropdownItemViewInfo item = mSourceViewInfoList.get(i);
            final PropertyModel model = item.model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.COLOR_SCHEME, mBrandedColorScheme);
            model.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, deviceType);

            // Add the background to suggestions.
            if (item.processor.allowBackgroundRounding()) {
                model.set(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED,
                        !inDropdownItemBackgroundRoundingGroup);
                // The default value is false, so we do not need to assign false to
                // BG_BOTTOM_CORNER_ROUNDED here.

                prevSuggestionWithBackground = item;
                inDropdownItemBackgroundRoundingGroup = true;
            } else {
                // If the current suggestion does not support background, we should round corner the
                // bottom of the previous suggestion's background.
                if (prevSuggestionWithBackground != null) {
                    prevSuggestionWithBackground.model.set(
                            DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, true);
                }
                inDropdownItemBackgroundRoundingGroup = false;
            }

            final boolean groupIsDefaultCollapsed = getGroupCollapsedState(item.groupId);
            if (!groupIsDefaultCollapsed || isGroupHeaderWithId(item, item.groupId)) {
                suggestionsList.add(item);
            }
        }

        // round the bottom corners of the last suggestion.
        if (prevSuggestionWithBackground != null) {
            prevSuggestionWithBackground.model.set(
                    DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, true);
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
     * Remove all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void removeSuggestionsForGroup(int groupId) {
        int index;
        int count = 0;

        for (index = mManagedModel.size() - 1; index >= 0; index--) {
            DropdownItemViewInfo viewInfo = (DropdownItemViewInfo) mManagedModel.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                break;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }
        if (count > 0) {
            // Skip group header when dropping items.
            mManagedModel.removeRange(index + 1, count);
        }
    }

    /**
     * Insert all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void insertSuggestionsForGroup(int groupId) {
        int insertPosition = 0;

        // Search for the insert position.
        // Iterate through all *available* view infos until we find the first element that we
        // should insert. To determine the insertion point we skip past all *displayed* view
        // infos that were also preceding elements that we want to insert.
        for (; insertPosition < mManagedModel.size(); insertPosition++) {
            final DropdownItemViewInfo viewInfo =
                    (DropdownItemViewInfo) mManagedModel.get(insertPosition);
            // Insert suggestions directly below their header.
            if (isGroupHeaderWithId(viewInfo, groupId)) break;
        }

        // Check if reached the end of the list.
        if (insertPosition == mManagedModel.size()) return;

        // insertPosition points to header - advance the index and see if we already have
        // elements belonging to that group on the list.
        insertPosition++;
        if (insertPosition < mManagedModel.size()
                && ((DropdownItemViewInfo) mManagedModel.get(insertPosition)).groupId == groupId) {
            return;
        }

        // Find elements to insert.
        int firstElementIndex = -1;
        int count = 0;
        for (int index = 0; index < mSourceViewInfoList.size(); index++) {
            final DropdownItemViewInfo viewInfo = mSourceViewInfoList.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                firstElementIndex = index + 1;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }

        if (count != 0 && firstElementIndex != -1) {
            mManagedModel.addAll(
                    mSourceViewInfoList.subList(firstElementIndex, firstElementIndex + count),
                    insertPosition);
        }
    }
}

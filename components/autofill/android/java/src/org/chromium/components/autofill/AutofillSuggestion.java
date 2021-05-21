// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.chromium.ui.DropdownItemBase;

/**
 * Autofill suggestion container used to store information needed for each Autofill popup entry.
 */
public class AutofillSuggestion extends DropdownItemBase {
    private final String mLabel;
    private final String mSublabel;
    private final String mItemTag;
    private final int mIconId;
    private final boolean mIsIconAtStart;
    private final int mSuggestionId;
    private final boolean mIsDeletable;
    private final boolean mIsMultilineLabel;
    private final boolean mIsBoldLabel;
    private final String mFeatureForIPH;

    /**
     * Constructs a Autofill suggestion container.
     *
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param itemTag The tag for the autofill suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or
     *               {@code DropdownItem.NO_ICON} for no icon.
     * @param isIconAtStart {@code true} if {@code iconId} is displayed before {@code label}.
     * @param suggestionId The type of suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param isMultilineLabel Whether the label is displayed over multiple lines.
     * @param isBoldLabel Whether the label is displayed in {@code Typeface.BOLD}.
     * @param featureForIPH The IPH feature for the autofill suggestion. If present, it'll be
     *         attempted to be shown in the keyboard accessory.
     */
    public AutofillSuggestion(String label, String sublabel, String itemTag, int iconId,
            boolean isIconAtStart, int suggestionId, boolean isDeletable, boolean isMultilineLabel,
            boolean isBoldLabel, String featureForIPH) {
        mLabel = label;
        mSublabel = sublabel;
        mItemTag = itemTag;
        mIconId = iconId;
        mIsIconAtStart = isIconAtStart;
        mSuggestionId = suggestionId;
        mIsDeletable = isDeletable;
        mIsMultilineLabel = isMultilineLabel;
        mIsBoldLabel = isBoldLabel;
        mFeatureForIPH = featureForIPH;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    public String getSublabel() {
        return mSublabel;
    }

    @Override
    public String getItemTag() {
        return mItemTag;
    }

    @Override
    public int getIconId() {
        return mIconId;
    }

    @Override
    public boolean isMultilineLabel() {
        return mIsMultilineLabel;
    }

    @Override
    public boolean isBoldLabel() {
        return mIsBoldLabel;
    }

    @Override
    public int getLabelFontColorResId() {
        if (mSuggestionId == PopupItemId.ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE) {
            return R.color.insecure_context_payment_disabled_message_text;
        }
        return super.getLabelFontColorResId();
    }

    @Override
    public boolean isIconAtStart() {
        if (mIsIconAtStart) {
            return true;
        }
        return super.isIconAtStart();
    }

    public int getSuggestionId() {
        return mSuggestionId;
    }

    public boolean isDeletable() {
        return mIsDeletable;
    }

    public boolean isFillable() {
        // Negative suggestion ID indiciates a tool like "settings" or "scan credit card."
        // Non-negative suggestion ID indicates suggestions that can be filled into the form.
        return mSuggestionId >= 0;
    }

    public String getFeatureForIPH() {
        return mFeatureForIPH;
    }
}

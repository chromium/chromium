// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.DropdownItemBase;
import org.chromium.url.GURL;

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
    private final GURL mCustomIconUrl;
    @Nullable
    private final Bitmap mCustomIcon;

    /**
     * Constructs a Autofill suggestion container.
     *
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param itemTag The tag for the autofill suggestion. For keyboard accessory, this would be
     *         displayed as an IPH bubble. For the dropdown, this is shown below the secondary
     *         text.For example: For credit cards with offers, the item tag is set to indicate that
     *         the card has some cashback offer associated with it.
     * @param iconId The resource ID for the icon associated with the suggestion, or
     *               {@code DropdownItem.NO_ICON} for no icon.
     * @param isIconAtStart {@code true} if {@code iconId} is displayed before {@code label}.
     * @param suggestionId The type of suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param isMultilineLabel Whether the label is displayed over multiple lines.
     * @param isBoldLabel Whether the label is displayed in {@code Typeface.BOLD}.
     * @param featureForIPH The IPH feature for the autofill suggestion. If present, it'll be
     *         attempted to be shown in the keyboard accessory.
     *
     * Use the {@link AutofillSuggestion.Builder} instead.
     */
    @Deprecated
    public AutofillSuggestion(String label, String sublabel, String itemTag, int iconId,
            boolean isIconAtStart, int suggestionId, boolean isDeletable, boolean isMultilineLabel,
            boolean isBoldLabel, String featureForIPH) {
        this(label, sublabel, itemTag, iconId, isIconAtStart, suggestionId, isDeletable,
                isMultilineLabel, isBoldLabel, featureForIPH, /*customIconUrl=*/null,
                /*customIcon=*/null);
    }

    @VisibleForTesting
    public AutofillSuggestion(String label, String sublabel, String itemTag, int iconId,
            boolean isIconAtStart, int suggestionId, boolean isDeletable, boolean isMultilineLabel,
            boolean isBoldLabel, String featureForIPH, GURL customIconUrl,
            @Nullable Bitmap customIcon) {
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
        mCustomIconUrl = customIconUrl;
        mCustomIcon = customIcon;
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

    @Override
    public GURL getCustomIconUrl() {
        return mCustomIconUrl;
    }

    @Override
    @Nullable
    public Bitmap getCustomIcon() {
        return mCustomIcon;
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

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AutofillSuggestion)) {
            return false;
        }
        AutofillSuggestion other = (AutofillSuggestion) o;
        return this.mLabel.equals(other.mLabel) && this.mSublabel.equals(other.mSublabel)
                && this.mItemTag.equals(other.mItemTag) && this.mIconId == other.mIconId
                && this.mIsIconAtStart == other.mIsIconAtStart
                && this.mSuggestionId == other.mSuggestionId
                && this.mIsDeletable == other.mIsDeletable
                && this.mIsMultilineLabel == other.mIsMultilineLabel
                && this.mIsBoldLabel == other.mIsBoldLabel
                && this.mFeatureForIPH.equals(other.mFeatureForIPH)
                && this.mCustomIconUrl.equals(other.mCustomIconUrl)
                && this.mCustomIcon.sameAs(other.mCustomIcon);
    }

    public Builder toBuilder() {
        return new Builder()
                .setLabel(mLabel)
                .setSubLabel(mSublabel)
                .setItemTag(mItemTag)
                .setIconId(mIconId)
                .setIsIconAtStart(mIsIconAtStart)
                .setSuggestionId(mSuggestionId)
                .setIsDeletable(mIsDeletable)
                .setIsMultiLineLabel(mIsMultilineLabel)
                .setIsBoldLabel(mIsBoldLabel)
                .setFeatureForIPH(mFeatureForIPH)
                .setCustomIconUrl(mCustomIconUrl)
                .setCustomIcon(mCustomIcon);
    }

    /**
     * Builder for the {@link AutofillSuggestion}.
     */
    public static final class Builder {
        private int mIconId;
        private GURL mCustomIconUrl;
        private Bitmap mCustomIcon;
        private boolean mIsBoldLabel;
        private boolean mIsIconAtStart;
        private boolean mIsDeletable;
        private boolean mIsMultiLineLabel;
        private String mFeatureForIPH;
        private String mItemTag;
        private String mLabel;
        private String mSubLabel;
        private int mSuggestionId;

        public Builder setIconId(int iconId) {
            this.mIconId = iconId;
            return this;
        }

        public Builder setCustomIconUrl(GURL customIconUrl) {
            this.mCustomIconUrl = customIconUrl;
            return this;
        }

        public Builder setCustomIcon(Bitmap customIcon) {
            this.mCustomIcon = customIcon;
            return this;
        }

        public Builder setIsBoldLabel(boolean isBoldLabel) {
            this.mIsBoldLabel = isBoldLabel;
            return this;
        }

        public Builder setIsIconAtStart(boolean isIconAtStart) {
            this.mIsIconAtStart = isIconAtStart;
            return this;
        }

        public Builder setIsDeletable(boolean isDeletable) {
            this.mIsDeletable = isDeletable;
            return this;
        }

        public Builder setIsMultiLineLabel(boolean isMultiLineLabel) {
            this.mIsMultiLineLabel = isMultiLineLabel;
            return this;
        }

        public Builder setFeatureForIPH(String featureForIPH) {
            this.mFeatureForIPH = featureForIPH;
            return this;
        }

        public Builder setItemTag(String itemTag) {
            this.mItemTag = itemTag;
            return this;
        }

        public Builder setLabel(String label) {
            this.mLabel = label;
            return this;
        }

        public Builder setSubLabel(String subLabel) {
            this.mSubLabel = subLabel;
            return this;
        }

        public Builder setSuggestionId(int suggestionId) {
            this.mSuggestionId = suggestionId;
            return this;
        }

        public AutofillSuggestion build() {
            assert !mLabel.isEmpty() : "AutofillSuggestion requires the label to be set.";
            return new AutofillSuggestion(mLabel, mSubLabel, mItemTag, mIconId, mIsIconAtStart,
                    mSuggestionId, mIsDeletable, mIsMultiLineLabel, mIsBoldLabel, mFeatureForIPH,
                    mCustomIconUrl, mCustomIcon);
        }
    }
}

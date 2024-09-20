// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.DropdownItemBase;
import org.chromium.url.GURL;

import java.util.Objects;

/** Autofill suggestion container used to store information needed for each Autofill popup entry. */
public class AutofillSuggestion extends DropdownItemBase {
    private final String mLabel;
    @Nullable private final String mSecondaryLabel;
    private final String mSublabel;
    @Nullable private final String mSecondarySublabel;
    @Nullable private final String mItemTag;
    private final int mIconId;
    private final boolean mIsIconAtStart;
    private final int mSuggestionType;
    private final boolean mIsDeletable;
    private final boolean mIsMultilineLabel;
    private final boolean mIsBoldLabel;
    private final boolean mApplyDeactivatedStyle;
    private final boolean mShouldDisplayTermsAvailable;
    @Nullable private final String mFeatureForIPH;
    private final String mIPHDescriptionText;
    @Nullable private final GURL mCustomIconUrl;
    @Nullable private final Drawable mIconDrawable;

    /**
     * Constructs a Autofill suggestion container. Use the {@link AutofillSuggestion.Builder}
     * instead.
     *
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param itemTag The tag for the autofill suggestion. For keyboard accessory, this would be
     *     displayed as an IPH bubble. For the dropdown, this is shown below the secondary text.For
     *     example: For credit cards with offers, the item tag is set to indicate that the card has
     *     some cashback offer associated with it.
     * @param iconId The resource ID for the icon associated with the suggestion, or {@code
     *     DropdownItem.NO_ICON} for no icon.
     * @param isIconAtStart {@code true} if {@code iconId} is displayed before {@code label}.
     * @param popupItemId The type of suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param isMultilineLabel Whether the label is displayed over multiple lines.
     * @param isBoldLabel Whether the label is displayed in {@code Typeface.BOLD}.
     * @param applyDeactivatedStyle Whether to apply deactivated style to the suggestion.
     * @param shouldDisplayTermsAvailable Whether the terms message is displayed.
     * @param featureForIPH The IPH feature for the autofill suggestion. If present, it'll be
     *     attempted to be shown in the keyboard accessory.
     * @param customIconUrl The {@link GURL} for the custom icon, if any.
     * @param iconDrawable The {@link Drawable} for an icon, if any.
     */
    @VisibleForTesting
    public AutofillSuggestion(
            String label,
            @Nullable String secondaryLabel,
            String sublabel,
            @Nullable String secondarySublabel,
            @Nullable String itemTag,
            int iconId,
            boolean isIconAtStart,
            @SuggestionType int popupItemId,
            boolean isDeletable,
            boolean isMultilineLabel,
            boolean isBoldLabel,
            boolean applyDeactivatedStyle,
            boolean shouldDisplayTermsAvailable,
            @Nullable String featureForIPH,
            String iphDescriptionText,
            @Nullable GURL customIconUrl,
            @Nullable Drawable iconDrawable) {
        mLabel = label;
        mSecondaryLabel = secondaryLabel;
        mSublabel = sublabel;
        mSecondarySublabel = secondarySublabel;
        mItemTag = itemTag;
        mIconId = iconId;
        mIsIconAtStart = isIconAtStart;
        mSuggestionType = popupItemId;
        mIsDeletable = isDeletable;
        mIsMultilineLabel = isMultilineLabel;
        mIsBoldLabel = isBoldLabel;
        mApplyDeactivatedStyle = applyDeactivatedStyle;
        mShouldDisplayTermsAvailable = shouldDisplayTermsAvailable;
        mFeatureForIPH = featureForIPH;
        mIPHDescriptionText = iphDescriptionText;
        mCustomIconUrl = customIconUrl;
        mIconDrawable = iconDrawable;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    @Nullable
    public String getSecondaryLabel() {
        return mSecondaryLabel;
    }

    @Override
    public String getSublabel() {
        return mSublabel;
    }

    @Override
    @Nullable
    public String getSecondarySublabel() {
        return mSecondarySublabel;
    }

    @Override
    @Nullable
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
        if (mSuggestionType == SuggestionType.INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE) {
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
    @Nullable
    public GURL getCustomIconUrl() {
        return mCustomIconUrl;
    }

    @Override
    @Nullable
    public Drawable getIconDrawable() {
        return mIconDrawable;
    }

    public int getSuggestionType() {
        return mSuggestionType;
    }

    public boolean isDeletable() {
        return mIsDeletable;
    }

    public boolean isFillable() {
        return mSuggestionType == SuggestionType.ADDRESS_ENTRY
                || mSuggestionType == SuggestionType.CREDIT_CARD_ENTRY;
    }

    public boolean applyDeactivatedStyle() {
        return mApplyDeactivatedStyle;
    }

    public boolean shouldDisplayTermsAvailable() {
        return mShouldDisplayTermsAvailable;
    }

    @Nullable
    public String getFeatureForIPH() {
        return mFeatureForIPH;
    }

    public String getIPHDescriptionText() {
        return mIPHDescriptionText;
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
        return this.mLabel.equals(other.mLabel)
                && Objects.equals(this.mSecondaryLabel, other.mSecondaryLabel)
                && this.mSublabel.equals(other.mSublabel)
                && Objects.equals(this.mSecondarySublabel, other.mSecondarySublabel)
                && Objects.equals(this.mItemTag, other.mItemTag)
                && this.mIconId == other.mIconId
                && this.mIsIconAtStart == other.mIsIconAtStart
                && this.mSuggestionType == other.mSuggestionType
                && this.mIsDeletable == other.mIsDeletable
                && this.mIsMultilineLabel == other.mIsMultilineLabel
                && this.mIsBoldLabel == other.mIsBoldLabel
                && this.mApplyDeactivatedStyle == other.mApplyDeactivatedStyle
                && this.mShouldDisplayTermsAvailable == other.mShouldDisplayTermsAvailable
                && Objects.equals(this.mFeatureForIPH, other.mFeatureForIPH)
                && this.mIPHDescriptionText.equals(other.mIPHDescriptionText)
                && Objects.equals(this.mCustomIconUrl, other.mCustomIconUrl)
                && areIconsEqual(this.mIconDrawable, other.mIconDrawable);
    }

    public Builder toBuilder() {
        return new Builder()
                .setLabel(mLabel)
                .setSecondaryLabel(mSecondaryLabel)
                .setSubLabel(mSublabel)
                .setSecondarySubLabel(mSecondarySublabel)
                .setItemTag(mItemTag)
                .setIconId(mIconId)
                .setIsIconAtStart(mIsIconAtStart)
                .setSuggestionType(mSuggestionType)
                .setIsDeletable(mIsDeletable)
                .setIsMultiLineLabel(mIsMultilineLabel)
                .setIsBoldLabel(mIsBoldLabel)
                .setApplyDeactivatedStyle(mApplyDeactivatedStyle)
                .setShouldDisplayTermsAvailable(mShouldDisplayTermsAvailable)
                .setFeatureForIPH(mFeatureForIPH)
                .setIPHDescriptionText(mIPHDescriptionText)
                .setCustomIconUrl(mCustomIconUrl)
                .setIconDrawable(mIconDrawable);
    }

    /** Builder for the {@link AutofillSuggestion}. */
    public static final class Builder {
        private int mIconId;
        private GURL mCustomIconUrl;
        private Drawable mIconDrawable;
        private boolean mIsBoldLabel;
        private boolean mIsIconAtStart;
        private boolean mIsDeletable;
        private boolean mIsMultiLineLabel;
        private boolean mApplyDeactivatedStyle;
        private boolean mShouldDisplayTermsAvailable;
        private String mFeatureForIPH;
        private String mIPHDescriptionText;
        private String mItemTag;
        private String mLabel;
        private String mSecondaryLabel;
        private String mSubLabel;
        private String mSecondarySubLabel;
        private int mSuggestionType;

        public Builder setIconId(int iconId) {
            this.mIconId = iconId;
            return this;
        }

        public Builder setCustomIconUrl(GURL customIconUrl) {
            this.mCustomIconUrl = customIconUrl;
            return this;
        }

        public Builder setIconDrawable(Drawable iconDrawable) {
            this.mIconDrawable = iconDrawable;
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

        public Builder setApplyDeactivatedStyle(boolean applyDeactivatedStyle) {
            this.mApplyDeactivatedStyle = applyDeactivatedStyle;
            return this;
        }

        public Builder setShouldDisplayTermsAvailable(boolean shouldDisplayTermsAvailable) {
            this.mShouldDisplayTermsAvailable = shouldDisplayTermsAvailable;
            return this;
        }

        public Builder setFeatureForIPH(String featureForIPH) {
            this.mFeatureForIPH = featureForIPH;
            return this;
        }

        public Builder setIPHDescriptionText(String iphDescriptionText) {
            this.mIPHDescriptionText = iphDescriptionText;
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

        public Builder setSecondaryLabel(String secondaryLabel) {
            this.mSecondaryLabel = secondaryLabel;
            return this;
        }

        public Builder setSubLabel(String subLabel) {
            this.mSubLabel = subLabel;
            return this;
        }

        public Builder setSecondarySubLabel(String secondarySubLabel) {
            this.mSecondarySubLabel = secondarySubLabel;
            return this;
        }

        public Builder setSuggestionType(int popupItemId) {
            this.mSuggestionType = popupItemId;
            return this;
        }

        public AutofillSuggestion build() {
            assert mSuggestionType == SuggestionType.SEPARATOR || !TextUtils.isEmpty(mLabel)
                    : "Only separators may have an empty label.";
            assert (mSubLabel != null)
                    : "The AutofillSuggestion sublabel can be empty but never null.";
            return new AutofillSuggestion(
                    mLabel,
                    mSecondaryLabel,
                    mSubLabel,
                    mSecondarySubLabel,
                    mItemTag,
                    mIconId,
                    mIsIconAtStart,
                    mSuggestionType,
                    mIsDeletable,
                    mIsMultiLineLabel,
                    mIsBoldLabel,
                    mApplyDeactivatedStyle,
                    mShouldDisplayTermsAvailable,
                    mFeatureForIPH,
                    mIPHDescriptionText,
                    mCustomIconUrl,
                    mIconDrawable);
        }
    }

    public static boolean areIconsEqual(
            @Nullable Drawable iconDrawable1, @Nullable Drawable iconDrawable2) {
        if (iconDrawable1 == null) {
            return iconDrawable2 == null;
        }
        // If the icons are custom Bitmap images.
        if (iconDrawable1 instanceof BitmapDrawable) {
            if (iconDrawable2 instanceof BitmapDrawable) {
                return ((BitmapDrawable) iconDrawable1)
                        .getBitmap()
                        .sameAs(((BitmapDrawable) iconDrawable2).getBitmap());
            }
            return false;
        }
        // Icons with {@code iconId} which are fetched from resources are already checked for
        // equality.
        return true;
    }
}

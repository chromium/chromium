// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.DropdownItemBase;
import org.chromium.url.GURL;

import java.util.Objects;

/** Autofill suggestion container used to store information needed for each Autofill popup entry. */
@NullMarked
public class AutofillSuggestion extends DropdownItemBase {
    private final @Nullable String mLabel;
    private final @Nullable String mSecondaryLabel;
    private final String mSublabel;
    private final @Nullable String mSecondarySublabel;
    private final @Nullable String mVoiceOver;
    private final int mIconId;
    private final @SuggestionType int mSuggestionType;
    private final boolean mIsDeletable;
    private final boolean mApplyDeactivatedStyle;
    private final @Nullable String mFeatureForIph;
    private final @Nullable String mIphDescriptionText;
    private final @Nullable GURL mCustomIconUrl;
    private final @Nullable Payload mPayload;

    public sealed interface Payload permits AutofillProfilePayload, PaymentsPayload {}

    /**
     * Constructs a Autofill suggestion container. Use the {@link AutofillSuggestion.Builder}
     * instead.
     *
     * @param label The main label of the Autofill suggestion.
     * @param sublabel The describing sublabel of the Autofill suggestion.
     * @param voiceOver Voice over text read for the Autofill suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or {@code
     *     DropdownItem.NO_ICON} for no icon.
     * @param popupItemId The type of suggestion.
     * @param isDeletable Whether the item can be deleted by the user.
     * @param applyDeactivatedStyle Whether to apply deactivated style to the suggestion.
     * @param featureForIph The IPH feature for the autofill suggestion. If present, it'll be
     *     attempted to be shown in the keyboard accessory.
     * @param customIconUrl The {@link GURL} for the custom icon, if any.
     * @param payload Additional data passed with the suggestion.
     */
    @VisibleForTesting
    public AutofillSuggestion(
            @Nullable String label,
            @Nullable String secondaryLabel,
            String sublabel,
            @Nullable String secondarySublabel,
            @Nullable String voiceOver,
            int iconId,
            @SuggestionType int popupItemId,
            boolean isDeletable,
            boolean applyDeactivatedStyle,
            @Nullable String featureForIph,
            @Nullable String iphDescriptionText,
            @Nullable GURL customIconUrl,
            @Nullable Payload payload) {
        mLabel = label;
        mSecondaryLabel = secondaryLabel;
        mSublabel = sublabel;
        mSecondarySublabel = secondarySublabel;
        mVoiceOver = voiceOver;
        mIconId = iconId;
        mSuggestionType = popupItemId;
        mIsDeletable = isDeletable;
        mApplyDeactivatedStyle = applyDeactivatedStyle;
        mFeatureForIph = featureForIph;
        mIphDescriptionText = iphDescriptionText;
        mCustomIconUrl = customIconUrl;
        mPayload = payload;
    }

    @Override
    public @Nullable String getLabel() {
        return mLabel;
    }

    @Override
    public @Nullable String getSecondaryLabel() {
        return mSecondaryLabel;
    }

    @Override
    public String getSublabel() {
        return mSublabel;
    }

    @Override
    public @Nullable String getSecondarySublabel() {
        return mSecondarySublabel;
    }

    @Override
    public int getIconId() {
        return mIconId;
    }

    @Override
    public int getLabelFontColorResId() {
        if (mSuggestionType == SuggestionType.INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE) {
            return R.color.insecure_context_payment_disabled_message_text;
        }
        return super.getLabelFontColorResId();
    }

    @Override
    public @Nullable GURL getCustomIconUrl() {
        return mCustomIconUrl;
    }

    public @SuggestionType int getSuggestionType() {
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

    public @Nullable String getFeatureForIph() {
        return mFeatureForIph;
    }

    public @Nullable String getIphDescriptionText() {
        return mIphDescriptionText;
    }

    public @Nullable String getVoiceOver() {
        return mVoiceOver;
    }

    public @Nullable AutofillProfilePayload getAutofillProfilePayload() {
        if (mPayload instanceof AutofillProfilePayload) {
            return (AutofillProfilePayload) mPayload;
        }
        return null;
    }

    public @Nullable PaymentsPayload getPaymentsPayload() {
        if (mPayload instanceof PaymentsPayload) {
            return (PaymentsPayload) mPayload;
        }
        return null;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AutofillSuggestion other)) {
            return false;
        }
        return Objects.equals(this.mLabel, other.mLabel)
                && Objects.equals(this.mSecondaryLabel, other.mSecondaryLabel)
                && this.mSublabel.equals(other.mSublabel)
                && Objects.equals(this.mSecondarySublabel, other.mSecondarySublabel)
                && this.mIconId == other.mIconId
                && this.mSuggestionType == other.mSuggestionType
                && this.mIsDeletable == other.mIsDeletable
                && this.mApplyDeactivatedStyle == other.mApplyDeactivatedStyle
                && Objects.equals(this.mFeatureForIph, other.mFeatureForIph)
                && Objects.equals(this.mIphDescriptionText, other.mIphDescriptionText)
                && Objects.equals(this.mCustomIconUrl, other.mCustomIconUrl)
                && Objects.equals(this.mPayload, other.mPayload);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                this.mLabel,
                this.mSecondaryLabel,
                this.mSublabel,
                this.mSecondarySublabel,
                this.mIconId,
                this.mSuggestionType,
                this.mIsDeletable,
                this.mApplyDeactivatedStyle,
                this.mFeatureForIph,
                this.mIphDescriptionText,
                this.mCustomIconUrl,
                this.mPayload);
    }

    /** Builder for the {@link AutofillSuggestion}. */
    public static final class Builder {
        private int mIconId;
        private @Nullable GURL mCustomIconUrl;
        private boolean mIsDeletable;
        private boolean mApplyDeactivatedStyle;
        private @Nullable String mFeatureForIph;
        private @Nullable String mIphDescriptionText;
        private @Nullable String mLabel;
        private @Nullable String mSecondaryLabel;
        private @Nullable String mSubLabel;
        private @Nullable String mSecondarySubLabel;
        private @Nullable String mVoiceOver;
        private int mSuggestionType;
        private @Nullable Payload mPayload;

        public Builder setIconId(int iconId) {
            this.mIconId = iconId;
            return this;
        }

        public Builder setCustomIconUrl(GURL customIconUrl) {
            this.mCustomIconUrl = customIconUrl;
            return this;
        }

        public Builder setIsDeletable(boolean isDeletable) {
            this.mIsDeletable = isDeletable;
            return this;
        }

        public Builder setApplyDeactivatedStyle(boolean applyDeactivatedStyle) {
            this.mApplyDeactivatedStyle = applyDeactivatedStyle;
            return this;
        }

        public Builder setFeatureForIph(String featureForIph) {
            this.mFeatureForIph = featureForIph;
            return this;
        }

        public Builder setIphDescriptionText(String iphDescriptionText) {
            this.mIphDescriptionText = iphDescriptionText;
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

        public Builder setVoiceOver(String voiceOver) {
            this.mVoiceOver = voiceOver;
            return this;
        }

        public Builder setPayload(Payload payload) {
            this.mPayload = payload;
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
                    mVoiceOver,
                    mIconId,
                    mSuggestionType,
                    mIsDeletable,
                    mApplyDeactivatedStyle,
                    mFeatureForIph,
                    mIphDescriptionText,
                    mCustomIconUrl,
                    mPayload);
        }
    }
}

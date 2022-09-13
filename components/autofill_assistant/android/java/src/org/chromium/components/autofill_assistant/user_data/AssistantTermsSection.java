// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;

/**
 * The third party terms and conditions section of the Autofill Assistant payment request.
 */
public class AssistantTermsSection {
    interface Delegate {
        void onStateChanged(@AssistantTermsAndConditionsState int state);

        void onLinkClicked(int link);
    }

    private final Context mContext;
    private final View mView;
    private final AssistantChoiceList mTermsList;
    private final TextView mTermsAgree;
    @Nullable
    private final TextView mTermsRequireReview;
    private final TextView mPrivacyNotice;
    @Nullable
    private Delegate mDelegate;

    AssistantTermsSection(Context context, ViewGroup parent, boolean showAsSingleCheckbox) {
        mContext = context;
        mView = LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_payment_request_terms_and_conditions, parent, false);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        parent.addView(mView, lp);
        mTermsList = mView.findViewById(R.id.third_party_terms_list);

        // This will display the terms as a checkbox instead of radio buttons.
        mTermsList.setAllowMultipleChoices(showAsSingleCheckbox);

        mTermsAgree = new TextView(context);
        ApiCompatibilityUtils.setTextAppearance(
                mTermsAgree, R.style.TextAppearance_TextSmall_Secondary);
        mTermsAgree.setGravity(Gravity.CENTER_VERTICAL);
        mTermsList.addItem(mTermsAgree, /* hasEditButton= */ false, selected -> {
            if (selected) {
                if (mDelegate != null) {
                    mDelegate.onStateChanged(AssistantTermsAndConditionsState.ACCEPTED);
                }
            } else if (showAsSingleCheckbox && mDelegate != null) {
                mDelegate.onStateChanged(AssistantTermsAndConditionsState.NOT_SELECTED);
            }
        }, /* itemEditedListener= */ null);

        if (showAsSingleCheckbox) {
            mTermsRequireReview = null;
        } else {
            mTermsRequireReview = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    mTermsRequireReview, R.style.TextAppearance_TextSmall_Secondary);
            mTermsRequireReview.setGravity(Gravity.CENTER_VERTICAL);
            mTermsRequireReview.setTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW);
            mTermsList.addItem(mTermsRequireReview, /* hasEditButton= */ false, selected -> {
                if (selected && mDelegate != null) {
                    mDelegate.onStateChanged(AssistantTermsAndConditionsState.REQUIRES_REVIEW);
                }
            }, /* itemEditedListener= */ null);
        }

        mPrivacyNotice = mView.findViewById(R.id.collect_data_privacy_notice);
    }

    private void onTermsAndConditionsLinkClicked(int link) {
        if (mDelegate != null) {
            mDelegate.onLinkClicked(link);
        }
    }

    public void setTermsStatus(@AssistantTermsAndConditionsState int status) {
        switch (status) {
            case AssistantTermsAndConditionsState.NOT_SELECTED:
                mTermsList.setCheckedItem(null);
                break;
            case AssistantTermsAndConditionsState.ACCEPTED:
                mTermsList.setCheckedItem(mTermsAgree);
                break;
            case AssistantTermsAndConditionsState.REQUIRES_REVIEW:
                if (mTermsRequireReview != null) {
                    mTermsList.setCheckedItem(mTermsRequireReview);
                }
                break;
        }
    }

    public void setPaddings(int topPadding, int bottomPadding) {
        mView.setPadding(
                mView.getPaddingLeft(), topPadding, mView.getPaddingRight(), bottomPadding);
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    void setAcceptTermsAndConditionsText(String text) {
        if (TextUtils.isEmpty(text)) {
            mTermsList.setVisibility(View.GONE);
        } else {
            mTermsList.setVisibility(View.VISIBLE);
            AssistantTextUtils.applyVisualAppearanceTags(
                    mTermsAgree, text, this::onTermsAndConditionsLinkClicked);
        }
    }

    void setTermsRequireReviewText(String text) {
        if (mTermsRequireReview != null) {
            AssistantTextUtils.applyVisualAppearanceTags(
                    mTermsRequireReview, text, /* linkCallback= */ null);
        }
    }

    void setPrivacyNoticeText(String text) {
        AssistantTextUtils.applyVisualAppearanceTags(
                mPrivacyNotice, text, /* linkCallback= */ null);
        mPrivacyNotice.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
    }

    void useBackgroundlessPrivacyNotice() {
        int paddingStart = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_backgroundless_privacy_notice_padding_start);
        mPrivacyNotice.setPaddingRelative(paddingStart, mPrivacyNotice.getPaddingTop(),
                mPrivacyNotice.getPaddingEnd(), mPrivacyNotice.getPaddingBottom());
        mPrivacyNotice.setTextAppearance(R.style.TextAppearance_TextSmall_Secondary);
        mPrivacyNotice.setBackgroundResource(0);
    }

    View getView() {
        return mView;
    }

    void setVisible(boolean visible) {
        mView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }
}

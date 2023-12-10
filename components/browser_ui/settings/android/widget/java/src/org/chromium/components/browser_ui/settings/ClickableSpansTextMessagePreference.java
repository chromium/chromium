// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * A preference wrapper for {@link TextViewWithClickableSpans}, which makes the
 * {@link TextMessagePreference} with one or more ClickableSpans accessible.
 */
public class ClickableSpansTextMessagePreference extends ChromeBasePreference {
    private CharSequence mTitle;
    private CharSequence mSummary;

    private TextViewWithClickableSpans mTitleView;
    private TextViewWithClickableSpans mSummaryView;

    /** Constructor for inflating from XML. */
    public ClickableSpansTextMessagePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.clickable_spans_text_message_preference_layout);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mTitleView = (TextViewWithClickableSpans) holder.findViewById(R.id.title);
        mSummaryView = (TextViewWithClickableSpans) holder.findViewById(R.id.summary);

        if (!TextUtils.isEmpty(mTitle)) {
            mTitleView.setText(mTitle);
            mTitleView.setVisibility(View.VISIBLE);
        } else {
            mTitleView.setVisibility(View.GONE);
        }

        if (!TextUtils.isEmpty(mSummary)) {
            mSummaryView.setText(mSummary);
            mSummaryView.setVisibility(View.VISIBLE);
            mSummaryView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            mSummaryView.setVisibility(View.GONE);
        }
    }

    @Override
    public void setTitle(CharSequence title) {
        if (!TextUtils.equals(mTitle, title)) {
            mTitle = title;
            notifyChanged();
        }
    }

    @Override
    public void setTitle(int titleResId) {
        setTitle(getContext().getString(titleResId));
    }

    @Override
    public void setSummary(CharSequence summary) {
        if (!TextUtils.equals(mSummary, summary)) {
            mSummary = summary;
            notifyChanged();
        }
    }

    @Override
    public void setSummary(int summaryResId) {
        setSummary(getContext().getString(summaryResId));
    }
}

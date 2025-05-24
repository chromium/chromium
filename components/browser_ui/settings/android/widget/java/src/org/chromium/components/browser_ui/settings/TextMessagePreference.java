// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.UiUtils;

/** A preference that displays informational text, and a summary which can contain a link. */
@NullMarked
public class TextMessagePreference extends ChromeBasePreference {
    private @Nullable TextView mTitleView;
    private @Nullable TextView mSummaryView;
    private @Nullable Integer mLiveRegionMode;
    private @Nullable CharSequence mTitleContentDescription;
    private @Nullable CharSequence mSummaryContentDescription;

    /** Constructor for inflating from XML. */
    public TextMessagePreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mTitleView = (TextView) holder.findViewById(android.R.id.title);
        mSummaryView = (TextView) holder.findViewById(android.R.id.summary);
        if (mSummaryView != null && getSummary() != null) {
            UiUtils.maybeSetLinkMovementMethod(mSummaryView);
        }
        if (mLiveRegionMode != null) {
            setAccessibilityLiveRegion(mLiveRegionMode);
        }
        if (mTitleContentDescription != null) {
            setTitleContentDescription(mTitleContentDescription);
        }
        if (mSummaryContentDescription != null) {
            setSummaryContentDescription(mSummaryContentDescription);
        }
    }

    /**
     * @param description Set the a11y content description of the title TextView.
     */
    public void setTitleContentDescription(CharSequence description) {
        mTitleContentDescription = description;
        if (mTitleView == null) return;
        mTitleView.setContentDescription(description);
    }

    /**
     * @param description Set the a11y content description of the summary TextView.
     */
    public void setSummaryContentDescription(CharSequence description) {
        mSummaryContentDescription = description;
        if (mSummaryView == null) return;
        mSummaryView.setContentDescription(description);
    }

    /**
     * Sets the accessibility live region property on the views related to this preference.
     *
     * @param liveRegionMode One of View.ACCESSIBILITY_LIVE_REGION_NONE, POLITE, or ASSERTIVE.
     */
    public void setAccessibilityLiveRegion(int liveRegionMode) {
        mLiveRegionMode = liveRegionMode;
        if (mTitleView != null) {
            mTitleView.setAccessibilityLiveRegion(liveRegionMode);
        }
        if (mSummaryView != null) {
            mSummaryView.setAccessibilityLiveRegion(liveRegionMode);
        }
    }
}

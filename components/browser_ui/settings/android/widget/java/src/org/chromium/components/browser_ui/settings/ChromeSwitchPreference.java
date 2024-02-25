// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.SwitchPreferenceCompat;

/** A Chrome switch preference that supports managed preferences. */
public class ChromeSwitchPreference extends SwitchPreferenceCompat {
    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /** The View for this preference. */
    private View mView;

    /** The color resource ID for tinting of the view's background. */
    @ColorRes private Integer mBackgroundColorRes;

    /** Indicates if the preference uses a custom layout. */
    private final boolean mHasCustomLayout;

    // TOOD(crbug.com/1451550): This is an interim solution. In the long-term, we should migrate
    // away from a switch with dynamically changing summaries onto a radio group.
    /**
     * Text to use for a11y announcements of the `summary` label. This text is static and does not
     * change when the toggle is switched between on/off states.
     */
    private String mSummaryOverrideForScreenReader;

    public ChromeSwitchPreference(Context context) {
        this(context, null);
    }

    public ChromeSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mHasCustomLayout = ManagedPreferencesUtils.isCustomLayoutApplied(context, attrs);
    }

    /** Sets the ManagedPreferenceDelegate which will determine whether this preference is managed. */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ true,
                /* hasCustomLayout= */ mHasCustomLayout);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView title = (TextView) holder.findViewById(android.R.id.title);
        title.setSingleLine(false);

        TextView summary = (TextView) holder.findViewById(android.R.id.summary);
        View.AccessibilityDelegate summaryOverrideDelegate = null;
        if (mSummaryOverrideForScreenReader != null) {
            summaryOverrideDelegate =
                    new View.AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.setText(mSummaryOverrideForScreenReader);
                        }

                        @Override
                        public void onPopulateAccessibilityEvent(
                                View unusedHost, AccessibilityEvent event) {
                            // Intentionally not calling through to `super` to replace
                            // default announcement.
                            event.getText().add(mSummaryOverrideForScreenReader);
                        }
                    };
            summary.setAccessibilityDelegate(summaryOverrideDelegate);
        }

        // Use summary as title if title is empty.
        if (TextUtils.isEmpty(getTitle())) {
            title.setText(summary.getText());
            title.setVisibility(View.VISIBLE);
            if (summaryOverrideDelegate != null) {
                title.setAccessibilityDelegate(summaryOverrideDelegate);
            }
            summary.setVisibility(View.GONE);
        }

        mView = holder.itemView;
        updateBackground();

        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }

    /**
     * Sets the Color resource ID which will be used to set the color of the view.
     * @param colorRes
     */
    public void setBackgroundColor(@ColorRes int colorRes) {
        if (mBackgroundColorRes != null && mBackgroundColorRes == colorRes) return;
        mBackgroundColorRes = colorRes;
        updateBackground();
    }

    /**
     * Sets the text to use when a11y announces the `summary` label.
     *
     * @param text The text to use in both on/off states, overriding both `summaryOn` and
     *     `summaryOff` values.
     */
    public void setSummaryOverrideForScreenReader(String text) {
        mSummaryOverrideForScreenReader = text;
    }

    private void updateBackground() {
        if (mView == null || mBackgroundColorRes == null) return;
        mView.setBackgroundColor(
                AppCompatResources.getColorStateList(getContext(), mBackgroundColorRes)
                        .getDefaultColor());
    }
}

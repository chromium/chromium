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

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.SwitchPreferenceCompat;

/** A Chrome switch preference that supports managed preferences. */
public class ChromeSwitchPreference extends SwitchPreferenceCompat {
    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /** The View for this preference. */
    private View mView;

    /** The color for tinting of the view's background. */
    @ColorInt @Nullable private Integer mBackgroundColorInt;

    /** Indicates if the preference uses a custom layout. */
    private final boolean mHasCustomLayout;

    // TOOD(crbug.com/1451550): This is an interim solution. In the long-term, we should migrate
    // away from a switch with dynamically changing summaries onto a radio group.
    /**
     * Text to use for a11y announcements of the `summary` label. This text is static and does not
     * change when the toggle is switched between on/off states.
     */
    private String mSummaryOverrideForScreenReader;

    private boolean mUseSummaryAsTitle;

    public ChromeSwitchPreference(Context context) {
        this(context, null);
    }

    public ChromeSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mHasCustomLayout = ManagedPreferencesUtils.isCustomLayoutApplied(context, attrs);
        mUseSummaryAsTitle = true;
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
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
        if (mUseSummaryAsTitle && TextUtils.isEmpty(getTitle())) {
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
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }

    /**
     * Sets the color which will be used for the view background.
     *
     * @param colorInt The color for the background.
     */
    public void setBackgroundColor(@ColorInt int colorInt) {
        if (mBackgroundColorInt != null && mBackgroundColorInt == colorInt) return;
        mBackgroundColorInt = colorInt;
        updateBackground();
    }

    /** Returns the background color of the preference. */
    public @Nullable @ColorInt Integer getBackgroundColor() {
        return mBackgroundColorInt;
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

    /** Controls whether the summary is used as title when the title is empty. */
    public void setUseSummaryAsTitle(boolean value) {
        mUseSummaryAsTitle = value;
    }

    private void updateBackground() {
        if (mView == null || mBackgroundColorInt == null) return;
        mView.setBackgroundColor(mBackgroundColorInt);
    }
}

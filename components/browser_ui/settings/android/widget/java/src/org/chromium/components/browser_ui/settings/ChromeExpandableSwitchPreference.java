// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.CheckableImageView;

/** A switch preference that can be expanded to show more details. */
@NullMarked
public class ChromeExpandableSwitchPreference extends ChromeSwitchPreference {
    private boolean mExpanded;
    @LayoutRes private final int mExpandedContentLayoutResId;
    private @Nullable Drawable mDrawable;

    public ChromeExpandableSwitchPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.chrome_expandable_switch_preference);
        TypedArray a =
                context.obtainStyledAttributes(attrs, R.styleable.ChromeExpandableSwitchPreference);
        mExpandedContentLayoutResId =
                a.getResourceId(
                        R.styleable.ChromeExpandableSwitchPreference_expandedContentLayout, 0);
        a.recycle();
        assert mExpandedContentLayoutResId != 0;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        if (mDrawable == null) {
            mDrawable = SettingsUtils.createExpandArrow(getContext());
        }
        CheckableImageView expandButton =
                (CheckableImageView) holder.findViewById(R.id.expandable_switch_expand_icon);
        if (expandButton != null) {
            expandButton.setImageDrawable(mDrawable);
            expandButton.setChecked(mExpanded);
            expandButton.setOnClickListener(v -> setExpanded(!mExpanded));
        }
        View expandedArea = holder.findViewById(R.id.expandable_switch_expanded_area);
        if (expandedArea == null && mExpanded) {
            ViewStub stub =
                    (ViewStub) holder.findViewById(R.id.expandable_switch_expanded_area_stub);
            if (stub != null) {
                stub.setLayoutResource(mExpandedContentLayoutResId);
                expandedArea = stub.inflate();
            }
        }
        if (expandedArea != null) {
            expandedArea.setVisibility(mExpanded ? View.VISIBLE : View.GONE);
            // Catch the click event on the expanded area to prevent it from propagating to the
            // parent view. This prevents the preference from toggling when the user interacts
            // with the expanded content.
            expandedArea.setOnClickListener(v -> {});
        }
        TextView summary = (TextView) holder.findViewById(android.R.id.summary);
        if (summary != null) {
            summary.setVisibility(getSummary() != null ? View.VISIBLE : View.GONE);
            if (getSummary() instanceof SpannableString) {
                summary.setMovementMethod(LinkMovementMethod.getInstance());
            }
        }
        updatePreferenceContentDescription(holder.itemView);
    }

    @Override
    public void onClick() {
        setExpanded(!isExpanded());
    }

    /**
     * Set the expanded/collapsed state for the preference.
     *
     * @param expanded The new expanded state.
     */
    public void setExpanded(boolean expanded) {
        if (mExpanded == expanded) return;
        mExpanded = expanded;
        notifyChanged();
    }

    /** Returns whether the preference is expanded. */
    public boolean isExpanded() {
        return mExpanded;
    }

    private void updatePreferenceContentDescription(View view) {
        // For accessibility, read out the whole title and whether the group is collapsed/expanded.
        String collapseOrExpandedText =
                getContext()
                        .getString(
                                mExpanded
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group);
        String description = getTitle() + ", " + collapseOrExpandedText;
        view.setContentDescription(description);
        if (view.isAccessibilityFocused()) {
            view.sendAccessibilityEvent(AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION);
        }
    }
}

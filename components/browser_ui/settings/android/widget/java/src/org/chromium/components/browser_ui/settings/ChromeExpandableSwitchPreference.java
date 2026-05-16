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
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceViewHolder;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.CheckableImageView;

/** A switch preference that can be expanded to show more details. */
@NullMarked
public class ChromeExpandableSwitchPreference extends ChromeSwitchPreference {
    /** Interface to be notified when the expanded area is bound. */
    public interface OnBindExpandedAreaListener {
        /**
         * Called when the expanded area is bound to the view holder.
         *
         * @param expandedArea The view that was bound (and potentially inflated).
         */
        void onBindExpandedArea(View expandedArea);
    }

    private boolean mExpanded;
    @LayoutRes private final int mExpandedContentLayoutResId;
    private @Nullable Drawable mDrawable;
    private @Nullable OnBindExpandedAreaListener mOnBindExpandedAreaListener;

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

    /**
     * Sets the listener to be notified when the expanded area is bound.
     *
     * @param listener The listener to set.
     */
    public void setOnBindExpandedAreaListener(@Nullable OnBindExpandedAreaListener listener) {
        mOnBindExpandedAreaListener = listener;
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        // We rely on isEnabled() in onBindViewHolder to apply custom styling
        // for disabled state while keeping links clickable.
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView title = (TextView) holder.findViewById(android.R.id.title);
        TextView summary = (TextView) holder.findViewById(android.R.id.summary);

        if (!isEnabled()) {
            // Disable the whole preference view hierarchy.
            ViewUtils.setEnabledRecursive(holder.itemView, false);

            // TODO(crbug.com/513357574): Handle accessibility for disabled preferences properly.
            // Just graying it out visually might not be enough for TalkBack.

            // Re-enable the summary view specifically to allow hyperlinks within it to remain
            // clickable, even though the rest of the preference is styled as disabled.
            if (summary != null) {
                summary.setEnabled(true);
            }

            // Use theme color for disabled state instead of manual alpha manipulation.
            int disabledColor = getDisabledColor();

            if (title != null) {
                title.setTextColor(disabledColor);
            }

            if (summary != null) {
                summary.setTextColor(disabledColor);
            }
        } else {
            // Re-enable the whole preference view hierarchy.
            ViewUtils.setEnabledRecursive(holder.itemView, true);

            // Restore default text colors.
            if (title != null) {
                title.setTextColor(
                        ContextCompat.getColor(getContext(), R.color.default_text_color_list));
            }

            if (summary != null) {
                summary.setTextColor(
                        ContextCompat.getColor(
                                getContext(), R.color.default_text_color_secondary_list));
            }
        }

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
            if (!isEnabled()) {
                ViewUtils.setEnabledRecursive(expandedArea, false);
            }
            // Call the listener after setting up the view and handling disabled state,
            // so the listener can override the enabled state of specific child views if needed.
            if (mExpanded && mOnBindExpandedAreaListener != null) {
                mOnBindExpandedAreaListener.onBindExpandedArea(expandedArea);
            }
        }

        if (summary != null) {
            summary.setVisibility(getSummary() != null ? View.VISIBLE : View.GONE);
            if (getSummary() instanceof SpannableString) {
                summary.setMovementMethod(LinkMovementMethod.getInstance());
            }
        }
        MaterialSwitch switchView =
                (MaterialSwitch) holder.findViewById(android.R.id.switch_widget);
        if (switchView != null) {
            switchView.setOnCheckedChangeListener(null);
            switchView.setChecked(isChecked());
            switchView.setOnCheckedChangeListener(
                    (buttonView, isCheckedArg) -> {
                        if (!callChangeListener(isCheckedArg)) {
                            buttonView.setChecked(!isCheckedArg);
                            return;
                        }
                        setChecked(isCheckedArg);
                    });
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

    public int getDisabledColor() {
        // Read the disabled color from resources directly.
        return getContext().getColor(R.color.default_text_color_disabled_list);
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

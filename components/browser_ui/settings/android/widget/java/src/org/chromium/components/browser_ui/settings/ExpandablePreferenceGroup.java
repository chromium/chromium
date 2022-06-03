// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceViewHolder;

import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.widget.CheckableImageView;

/**
 * A preference category that can be in either expanded or collapsed state. It shows expand/collapse
 * arrow and changes content description for a11y according to the current state. Use
 * {@link #setExpanded} to toggle collapsed/expanded state. Please note that this preference group
 * won't modify the set of children preferences on expanded state change.
 */
public class ExpandablePreferenceGroup extends PreferenceGroup {
    private boolean mExpanded = true;
    private Drawable mDrawable;

    public ExpandablePreferenceGroup(Context context, AttributeSet attrs) {
        super(context, attrs, R.attr.preferenceStyle);

        setWidgetLayoutResource(R.layout.checkable_image_view_widget);
    }

    /** Returns whether the preference group is expanded. */
    public boolean isExpanded() {
        return mExpanded;
    }

    /**
     * Set the expanded/collapsed state for the preference group.
     * @param expanded The new expanded state.
     */
    public final void setExpanded(boolean expanded) {
        if (mExpanded == expanded) return;
        mExpanded = expanded;
        onExpandedChanged(expanded);
        notifyChanged();
    }

    /** Subclasses may override this method to handle changes to the expanded/collapsed state. */
    protected void onExpandedChanged(boolean expanded) {}

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mDrawable == null) {
            mDrawable = createDrawable(getContext());
        }
        CheckableImageView imageView =
                (CheckableImageView) holder.findViewById(R.id.checkable_image_view);
        imageView.setImageDrawable(mDrawable);
        imageView.setChecked(mExpanded);

        // For accessibility, read out the whole title and whether the group is collapsed/expanded.
        View view = holder.itemView;
        String description = getTitle()
                + getContext().getResources().getString(mExpanded
                                ? R.string.accessibility_expanded_group
                                : R.string.accessibility_collapsed_group);
        view.setContentDescription(description);
    }

    private static Drawable createDrawable(Context context) {
        StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
        StateListDrawableBuilder.State checked = builder.addState(
                R.drawable.ic_expand_less_black_24dp, android.R.attr.state_checked);
        StateListDrawableBuilder.State unchecked =
                builder.addState(R.drawable.ic_expand_more_black_24dp);
        builder.addTransition(
                checked, unchecked, R.drawable.transition_expand_less_expand_more_black_24dp);
        builder.addTransition(
                unchecked, checked, R.drawable.transition_expand_more_expand_less_black_24dp);

        Drawable tintableDrawable = DrawableCompat.wrap(builder.build());
        DrawableCompat.setTintList(tintableDrawable,
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));
        return tintableDrawable;
    }
}

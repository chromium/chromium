// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.floating_pill;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

import java.util.ArrayList;
import java.util.List;

/**
 * A floating pill: a fully rounded surface, meant to be placed in a {@link FloatingPillContainer},
 * which paints its glow. From start to end it holds:
 *
 * <ul>
 *   <li>Custom content: child views declared in XML or added with {@link #addView}. They go before
 *       the buttons, unless added at an explicit index, which is used as is. Custom content sets
 *       its own margins.
 *   <li>Buttons, see {@link #addButton}.
 * </ul>
 *
 * <p>Its default look comes from the {@code FloatingPill} style, which XML attributes override.
 */
@NullMarked
public class FloatingPillView extends LinearLayout {
    // The buttons added with addButton(), which stay at the end.
    private final List<View> mButtons = new ArrayList<>();

    public FloatingPillView(Context context) {
        this(context, null);
    }

    public FloatingPillView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs, 0, R.style.FloatingPill);
    }

    /**
     * Adds a button at the end of the pill.
     *
     * @param icon The button's icon.
     * @param contentDescription Describes the button for accessibility, and is its tooltip.
     * @param onClickListener Called when the button is clicked.
     * @return The button, e.g. to update its icon. When updating its content description, also
     *     update its tooltip to match.
     */
    public ImageButton addButton(
            Drawable icon, CharSequence contentDescription, View.OnClickListener onClickListener) {
        ImageButton button =
                (ImageButton)
                        LayoutInflater.from(getContext())
                                .inflate(
                                        R.layout.floating_pill_button,
                                        this,
                                        /* attachToRoot= */ false);
        button.setImageDrawable(icon);
        button.setContentDescription(contentDescription);
        button.setTooltipText(contentDescription);
        button.setOnClickListener(onClickListener);
        button.setPointerIcon(PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_HAND));
        // An explicit index puts the button at the end, after any custom content.
        addView(button, getChildCount());
        mButtons.add(button);
        return button;
    }

    /** Puts views other than buttons before the buttons, unless given an explicit index. */
    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        if (index < 0) index = getChildCount() - mButtons.size();
        super.addView(child, index, params);
    }

    @Override
    public void onViewRemoved(View child) {
        super.onViewRemoved(child);
        mButtons.remove(child);
    }
}

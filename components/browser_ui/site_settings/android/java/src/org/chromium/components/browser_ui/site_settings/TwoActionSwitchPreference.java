// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/** A 2-action switch Preference used for location permission in SiteSettings. */
@NullMarked
public class TwoActionSwitchPreference extends ChromeSwitchPreference {
    private View.@Nullable OnClickListener mPrimaryButtonClickListener;

    public TwoActionSwitchPreference(Context context) {
        this(context, null);
    }

    public TwoActionSwitchPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        // Override the existing widget layout resource to include a divider + switch.
        setWidgetLayoutResource(R.layout.two_action_switch_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Override the default ChromeSwitchPreference click listener.
        if (mPrimaryButtonClickListener != null) {
            holder.itemView.setOnClickListener(mPrimaryButtonClickListener);
        }

        View switchContainer = holder.itemView.findViewById(R.id.switch_container);
        assert switchContainer != null;
        switchContainer.setOnClickListener(v -> onSwitchClick());
    }

    private void onSwitchClick() {
        this.onClick();
    }

    /** Sets the primary click listener for the left hand side of the preference. */
    public void setPrimaryButtonClickListener(View.@Nullable OnClickListener clickListener) {
        if (mPrimaryButtonClickListener != clickListener) {
            mPrimaryButtonClickListener = clickListener;
            this.notifyChanged();
        }
    }
}

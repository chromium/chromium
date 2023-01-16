// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.SwitchPreferenceCompat;

/**
 * A Chrome switch preference that supports managed preferences.
 */
public class ChromeSwitchPreference extends SwitchPreferenceCompat {
    private ManagedPreferenceDelegate mManagedPrefDelegate;
    /** The View for this preference. */
    private View mView;
    /** The color resource ID for tinting of the view's background. */
    @ColorRes
    private Integer mBackgroundColorRes;

    /** Indicates if the preference uses a custom layout. */
    private final boolean mHasCustomLayout;

    public ChromeSwitchPreference(Context context) {
        this(context, null);
    }

    public ChromeSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mHasCustomLayout = ManagedPreferencesUtils.isCustomLayoutApplied(context, attrs);
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(mManagedPrefDelegate, this,
                /*allowManagedIcon=*/true, /*hasCustomLayout=*/mHasCustomLayout);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView title = (TextView) holder.findViewById(android.R.id.title);
        title.setSingleLine(false);

        // Use summary as title if title is empty.
        if (TextUtils.isEmpty(getTitle())) {
            TextView summary = (TextView) holder.findViewById(android.R.id.summary);
            title.setText(summary.getText());
            title.setVisibility(View.VISIBLE);
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

    private void updateBackground() {
        if (mView == null || mBackgroundColorRes == null) return;
        mView.setBackgroundColor(
                AppCompatResources.getColorStateList(getContext(), mBackgroundColorRes)
                        .getDefaultColor());
    }
}

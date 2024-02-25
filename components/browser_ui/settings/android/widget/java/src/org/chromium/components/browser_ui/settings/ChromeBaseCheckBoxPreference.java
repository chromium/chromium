// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.CheckBoxPreference;
import androidx.preference.PreferenceViewHolder;

/** Contains the basic functionality that should be shared by all CheckBoxPreference in Chrome. */
public class ChromeBaseCheckBoxPreference extends CheckBoxPreference {
    /** Indicates if the preference uses a custom layout. */
    private final boolean mHasCustomLayout;

    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /** Constructor for use in Java. */
    public ChromeBaseCheckBoxPreference(Context context) {
        this(context, null);
    }

    /** Constructor for inflating from XML. */
    public ChromeBaseCheckBoxPreference(Context context, AttributeSet attrs) {
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
        ((TextView) holder.findViewById(android.R.id.title)).setSingleLine(false);
        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }
}

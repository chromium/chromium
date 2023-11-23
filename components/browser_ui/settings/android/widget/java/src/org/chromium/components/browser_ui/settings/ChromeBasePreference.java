// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;

/**
 * A preference that supports some Chrome-specific customizations:
 *
 * <p>This preference supports being managed. If this preference is managed (as determined by its
 * ManagedPreferenceDelegate), it updates its appearance and behavior appropriately: shows an
 * enterprise icon, disables clicks, etc.
 *
 * <p>This preference can have a multiline title.
 *
 * <p>This preference can set an icon color in XML through app:iconTint. Note that if a
 * ColorStateList is set, only the default color will be used.
 */
public class ChromeBasePreference extends Preference {
    private ColorStateList mIconTint;
    private ManagedPreferenceDelegate mManagedPrefDelegate;

    /** Indicates if the preference uses a custom layout. */
    private final boolean mHasCustomLayout;

    /** When null, the default Preferences Support Library logic will be used to determine dividers. */
    @Nullable private Boolean mDividerAllowedAbove;

    @Nullable private Boolean mDividerAllowedBelow;
    @Nullable private String mUserAction;

    /** Constructor for use in Java. */
    public ChromeBasePreference(Context context) {
        this(context, null);
    }

    /** Constructor for inflating from XML. */
    public ChromeBasePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setSingleLineTitle(false);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ChromeBasePreference);
        mIconTint = a.getColorStateList(R.styleable.ChromeBasePreference_iconTint);
        mUserAction = a.getString(R.styleable.ChromeBasePreference_userAction);
        a.recycle();

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

        Drawable icon = getIcon();
        if (icon != null && mIconTint != null) {
            icon.setColorFilter(mIconTint.getDefaultColor(), PorterDuff.Mode.SRC_IN);
        }

        ManagedPreferencesUtils.onBindViewToPreference(mManagedPrefDelegate, this, holder.itemView);

        if (mDividerAllowedAbove != null) {
            holder.setDividerAllowedAbove(mDividerAllowedAbove);
        }
        if (mDividerAllowedBelow != null) {
            holder.setDividerAllowedBelow(mDividerAllowedBelow);
        }
    }

    public void setDividerAllowedAbove(boolean allowed) {
        mDividerAllowedAbove = allowed;
    }

    public void setDividerAllowedBelow(boolean allowed) {
        mDividerAllowedBelow = allowed;
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        if (mUserAction != null) {
            RecordUserAction.record(mUserAction);
        }
        super.onClick();
    }
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A preference with a clickable "Learn more" link.
 *
 * <p>During {@link androidx.preference.PreferenceFragmentCompat#onCreatePreferences}, call {@link
 * #setOnPreferenceClickListener} to configure the link click behavior.
 */
@NullMarked
public class LearnMorePreference extends Preference {
    private @Nullable TextView mTitleView;
    private @Nullable CharSequence mLearnMoreSettingName;

    public LearnMorePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setTitle(R.string.learn_more);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mTitleView = (TextView) holder.findViewById(android.R.id.title);
        mTitleView.setTextAppearance(R.style.TextAppearance_TextLarge_Link);
        mTitleView.setClickable(true);
        mTitleView.setOnClickListener(
                v -> assumeNonNull(getOnPreferenceClickListener()).onPreferenceClick(this));
        if (mLearnMoreSettingName != null) {
            setLearnMoreSettingName(mLearnMoreSettingName);
        }
    }

    /**
     * @param settingName Sets the setting-specific name used within the 'Learn more about...'
     *     accessibility description.
     */
    public void setLearnMoreSettingName(CharSequence settingName) {
        mLearnMoreSettingName = settingName;
        if (mTitleView == null) return;
        mTitleView.setContentDescription(
                getContext().getString(R.string.learn_more_about_setting, mLearnMoreSettingName));
    }
}

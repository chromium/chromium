// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;

/**
 * Preference for websites which belongs to the RWS (Related website sets).
 *
 * <p>TODO(b/328267162): Implement full functionality of the RWS preference.
 *
 * <p>Note: We leverage the existing implementation of the {@link WebsiteRowPreference} limiting its
 * functionality for now (remove delete button) + make it not selectable.
 */
// TODO(crbug.com/394302220): Investigate removing/refactoring during RWS clean-up.
@NullMarked
public class RwsRowPreference extends WebsiteRowPreference {
    private final SiteSettingsDelegate mSiteSettingsDelegate;

    RwsRowPreference(
            Context context,
            SiteSettingsDelegate siteSettingsDelegate,
            WebsiteEntry siteEntry,
            LayoutInflater layoutInflater) {
        // RwsRowPreference displays websites in a related set under a related sites header
        // making the membership label redundant
        super(
                context,
                siteSettingsDelegate,
                siteEntry,
                layoutInflater,
                /* showRwsMembershipLabels= */ false);
        mSiteSettingsDelegate = siteSettingsDelegate;
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (!mSiteSettingsDelegate.shouldShowPrivacySandboxRwsUi()) {
            // Previous version of the RWS UI hides the delete button for the row
            ImageView button = (ImageView) holder.findViewById(R.id.image_view_widget);
            button.setVisibility(View.INVISIBLE);
        }
    }
}

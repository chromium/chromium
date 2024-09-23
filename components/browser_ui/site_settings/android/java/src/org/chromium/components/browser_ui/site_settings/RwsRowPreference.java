// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

/**
 * Preference for websites which belongs to the RWS (Related website sets).
 *
 * <p>TODO(b/328267162): Implement full functionality of the RWS preference.
 *
 * <p>Note: We leverage the existing implementation of the {@link WebsiteRowPreference} limiting its
 * functionality for now (remove delete button) + make it not selectable.
 */
public class RwsRowPreference extends WebsiteRowPreference {

    RwsRowPreference(
            Context context,
            SiteSettingsDelegate siteSettingsDelegate,
            WebsiteEntry siteEntry,
            LayoutInflater layoutInflater) {
        super(context, siteSettingsDelegate, siteEntry, layoutInflater);
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ImageView mButton = (ImageView) holder.findViewById(R.id.image_view_widget);
        mButton.setVisibility(View.INVISIBLE);
    }
}

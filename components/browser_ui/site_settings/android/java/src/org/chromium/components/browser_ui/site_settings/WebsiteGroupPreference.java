// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.format.Formatter;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;

/**
 * Used by {@link AllSiteSettings} to display a row for a group of sites or a single site.
 */
public class WebsiteGroupPreference extends ChromeImageViewPreference {
    private final SiteSettingsDelegate mSiteSettingsDelegate;
    private final WebsiteGroup mSiteGroup;

    private static final String HTTP = "http";

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    WebsiteGroupPreference(
            Context context, SiteSettingsDelegate siteSettingsDelegate, WebsiteGroup siteGroup) {
        super(context);
        mSiteSettingsDelegate = siteSettingsDelegate;
        mSiteGroup = siteGroup;

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));
        setTitle(mSiteGroup.getTitle());
        setImageView(
                R.drawable.ic_delete_white_24dp, R.string.webstorage_clear_data_dialog_title, null);
        updateSummary();
    }

    public boolean representsOneWebsite() {
        return mSiteGroup.hasOneOrigin();
    }

    public void putSingleSiteIntoExtras(String key) {
        if (!representsOneWebsite()) return;
        getExtras().putSerializable(key, mSiteGroup.getWebsites().get(0));
    }

    public void putGroupSiteIntoExtras(String key) {
        getExtras().putSerializable(key, mSiteGroup);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Manually apply ListItemStartIcon style to draw the outer circle in the right size.
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mSiteSettingsDelegate.getFaviconImageForURL(
                    mSiteGroup.getFaviconUrl(), this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }

    private void updateSummary() {
        String summary = "";

        long usage = mSiteGroup.getTotalUsage();
        if (usage > 0) {
            summary = Formatter.formatShortFileSize(getContext(), usage);
        }

        if (mSiteGroup.hasOneHttpOrigin()) {
            if (summary.isEmpty()) {
                summary = HTTP;
            } else {
                summary = String.format(
                        getContext().getString(R.string.summary_with_one_bullet), HTTP, summary);
            }
        }

        if (!summary.isEmpty()) {
            setSummary(summary);
        }
    }
}

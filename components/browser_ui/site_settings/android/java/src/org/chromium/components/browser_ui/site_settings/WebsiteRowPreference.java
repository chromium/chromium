// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.format.Formatter;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Used by {@link AllSiteSettings} to display a row for a group of sites or a single site.
 */
public class WebsiteRowPreference extends ChromeImageViewPreference {
    private final SiteSettingsDelegate mSiteSettingsDelegate;
    private final WebsiteEntry mSiteEntry;

    private static final String HTTP = "http";

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    WebsiteRowPreference(
            Context context, SiteSettingsDelegate siteSettingsDelegate, WebsiteEntry siteEntry) {
        super(context);
        mSiteSettingsDelegate = siteSettingsDelegate;
        mSiteEntry = siteEntry;

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));
        setTitle(mSiteEntry.getTitleForPreferenceRow());
        setImageView(
                R.drawable.ic_delete_white_24dp, R.string.webstorage_clear_data_dialog_title, null);
        updateSummary();
    }

    @SuppressWarnings("WrongConstant")
    public void handleClick(Bundle args) {
        getExtras().putSerializable(mSiteEntry instanceof Website
                        ? SingleWebsiteSettings.EXTRA_SITE
                        : GroupedWebsitesSettings.EXTRA_GROUP,
                mSiteEntry);
        setFragment(mSiteEntry instanceof Website ? SingleWebsiteSettings.class.getName()
                                                  : GroupedWebsitesSettings.class.getName());
        getExtras().putInt(SettingsNavigationSource.EXTRA_KEY,
                args.getInt(SettingsNavigationSource.EXTRA_KEY, SettingsNavigationSource.OTHER));
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
                    mSiteEntry.getFaviconUrl(), this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }

    private boolean isSiteEntryASingleHttpOrigin() {
        if (!(mSiteEntry instanceof Website)) return false;
        Website website = (Website) mSiteEntry;
        return website.getAddress().getOrigin().startsWith(UrlConstants.HTTP_URL_PREFIX);
    }

    private void updateSummary() {
        String summary = "";

        long usage = mSiteEntry.getTotalUsage();
        if (usage > 0) {
            summary = Formatter.formatShortFileSize(getContext(), usage);
        }

        int cookies = mSiteEntry.getNumberOfCookies();
        if (cookies > 0) {
            String cookie_str = getContext().getResources().getQuantityString(
                    R.plurals.cookies_count, cookies, cookies);
            if (summary.isEmpty()) {
                summary = cookie_str;
            } else {
                summary = String.format(getContext().getString(R.string.summary_with_one_bullet),
                        cookie_str, summary);
            }
        }

        // When a single HTTP origin is represented, mark it as such.
        if (isSiteEntryASingleHttpOrigin()) {
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

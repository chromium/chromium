// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.text.format.Formatter;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;

/**
 * A preference that displays a website's favicon and URL and, optionally, the amount of local
 * storage used by the site. This preference can also display an additional icon on the right side
 * of the preference. See {@link ChromeImageViewPreference} for more details on how this icon
 * can be used.
 */
class WebsitePreference extends ChromeImageViewPreference {
    private final SiteSettingsClient mSiteSettingsClient;
    private final Website mSite;
    private final SiteSettingsCategory mCategory;

    // TODO(crbug.com/1076571): Move these constants to dimens.xml
    private static final int FAVICON_PADDING_DP = 4;
    private static final int TEXT_SIZE_SP = 13;

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    WebsitePreference(Context context, SiteSettingsClient siteSettingsClient, Website site,
            SiteSettingsCategory category) {
        super(context);
        mSiteSettingsClient = siteSettingsClient;
        mSite = site;
        mCategory = category;
        setWidgetLayoutResource(R.layout.website_features);

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));

        refresh();
    }

    public void putSiteIntoExtras(String key) {
        getExtras().putSerializable(key, mSite);
    }

    public void putSiteAddressIntoExtras(String key) {
        getExtras().putSerializable(key, mSite.getAddress());
    }

    /**
     * Return the Website this object is representing.
     */
    public Website site() {
        return mSite;
    }

    /**
     * Returns the url of the site to fetch a favicon for.
     */
    private String faviconUrl() {
        String origin = mSite.getAddress().getOrigin();
        Uri uri = Uri.parse(origin);
        if (uri.getPort() != -1) {
            // Remove the port.
            uri = uri.buildUpon().authority(uri.getHost()).build();
        }
        return uri.toString();
    }

    private void refresh() {
        setTitle(mSite.getTitle());

        if (mSite.getEmbedder() == null) {
            PermissionInfo permissionInfo =
                    mSite.getPermissionInfo(mCategory.getContentSettingsType());
            if (permissionInfo != null && permissionInfo.isEmbargoed()) {
                setSummary(getContext().getString(R.string.automatically_blocked));
            }
            return;
        }
        String subtitleText;
        if (mSite.representsThirdPartiesOnSite()) {
            subtitleText = getContext().getString(
                    R.string.website_settings_third_party_cookies_exception_label);
        } else {
            subtitleText =
                    String.format(getContext().getString(R.string.website_settings_embedded_on),
                            mSite.getEmbedder().getTitle());
        }

        setSummary(subtitleText);
    }

    @Override
    public int compareTo(Preference preference) {
        if (!(preference instanceof WebsitePreference)) {
            return super.compareTo(preference);
        }
        WebsitePreference other = (WebsitePreference) preference;
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            return mSite.compareByStorageTo(other.mSite);
        }

        return mSite.compareByAddressTo(other.mSite);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView usageText = (TextView) holder.findViewById(R.id.usage_text);
        usageText.setVisibility(View.GONE);
        if (mCategory.showSites(SiteSettingsCategory.Type.USE_STORAGE)) {
            long totalUsage = mSite.getTotalUsage();
            if (totalUsage > 0) {
                usageText.setText(Formatter.formatShortFileSize(getContext(), totalUsage));
                usageText.setTextSize(TEXT_SIZE_SP);
                usageText.setVisibility(View.VISIBLE);
            }
        }

        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mSiteSettingsClient.getFaviconImageForURL(faviconUrl(), this::onFaviconAvailable);
            mFaviconFetched = true;
        }

        float density = getContext().getResources().getDisplayMetrics().density;
        int iconPadding = Math.round(FAVICON_PADDING_DP * density);
        View iconView = holder.findViewById(android.R.id.icon);
        iconView.setPadding(iconPadding, iconPadding, iconPadding, iconPadding);
    }

    private void onFaviconAvailable(Bitmap image) {
        if (image != null) {
            setIcon(new BitmapDrawable(getContext().getResources(), image));
        }
    }
}

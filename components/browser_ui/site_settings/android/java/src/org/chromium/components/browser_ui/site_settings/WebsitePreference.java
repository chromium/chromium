// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.format.Formatter;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.url.GURL;

/**
 * A preference that displays a website's favicon and URL and, optionally, the amount of local
 * storage used by the site. This preference can also display an additional icon on the right side
 * of the preference. See {@link ChromeImageViewPreference} for more details on how this icon
 * can be used.
 */
class WebsitePreference extends ChromeImageViewPreference {
    private final SiteSettingsDelegate mSiteSettingsDelegate;
    private final Website mSite;
    private final SiteSettingsCategory mCategory;

    // TODO(crbug.com/1076571): Move these constants to dimens.xml
    private static final int TEXT_SIZE_SP = 13;

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    // Finch param to allow subdomain settings for Request Desktop Site.
    static final String PARAM_SUBDOMAIN_SETTINGS = "SubdomainSettings";

    WebsitePreference(Context context, SiteSettingsDelegate siteSettingsClient, Website site,
            SiteSettingsCategory category) {
        super(context);
        mSiteSettingsDelegate = siteSettingsClient;
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
    private GURL faviconUrl() {
        String origin = mSite.getMainAddress().getOrigin();
        GURL uri = new GURL(origin);
        return UrlUtilities.clearPort(uri);
    }

    private void refresh() {
        setTitle(mSite.getTitle());

        if (mSiteSettingsDelegate.isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                && mSiteSettingsDelegate.isFirstPartySetsDataAccessEnabled()
                && mSite.getFPSCookieInfo() != null) {
            var fpsInfo = mSite.getFPSCookieInfo();
            setSummary(getContext().getResources().getQuantityString(
                    R.plurals.allsites_fps_list_summary, fpsInfo.getMembersCount(),
                    Integer.toString(fpsInfo.getMembersCount()), fpsInfo.getOwner()));
            return;
        }

        if (mSite.getEmbedder() == null) {
            if (mSite.isEmbargoed(mCategory.getContentSettingsType())) {
                setSummary(getContext().getString(R.string.automatically_blocked));
            } else if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE
                    && ContentFeatureMap.getInstance().getFieldTrialParamByFeatureAsBoolean(
                            ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS,
                            PARAM_SUBDOMAIN_SETTINGS, true)
                    && mSite.getAddress().getIsAnySubdomainPattern()) {
                setSummary(String.format(
                        getContext().getString(R.string.website_settings_domain_exception_label),
                        mSite.getAddress().getHost()));
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
        if (mCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE) {
            return mSite.compareByStorageTo(other.mSite);
        }

        return mSite.compareByAddressTo(other.mSite);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView usageText = (TextView) holder.findViewById(R.id.usage_text);
        usageText.setVisibility(View.GONE);
        if (mCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE) {
            long totalUsage = mSite.getTotalUsage();
            if (totalUsage > 0) {
                usageText.setText(Formatter.formatShortFileSize(getContext(), totalUsage));
                usageText.setTextSize(TEXT_SIZE_SP);
                usageText.setVisibility(View.VISIBLE);
            }
        }

        // Manually apply ListItemStartIcon style to draw the outer circle in the right size.
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mSiteSettingsDelegate.getFaviconImageForURL(faviconUrl(), this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }
}

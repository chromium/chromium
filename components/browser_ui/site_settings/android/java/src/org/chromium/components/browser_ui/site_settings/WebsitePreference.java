// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.format.Formatter;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/**
 * A preference that displays a website's favicon and URL and, optionally, the amount of local
 * storage used by the site. This preference can also display an additional icon on the right side
 * of the preference. See {@link ChromeImageViewPreference} for more details on how this icon
 * can be used.
 */
class WebsitePreference extends ChromeImageViewPreference {
    protected final SiteSettingsDelegate mSiteSettingsDelegate;
    protected final Website mSite;
    protected final SiteSettingsCategory mCategory;
    private Runnable mRefreshZoomsListFunction;

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    private OnStorageAccessWebsiteDetailsRequested mStorageAccessSettingsPageListener;

    /** Used to notify storage access website details subpage requests. */
    public interface OnStorageAccessWebsiteDetailsRequested {
        /** Notify that website details subpage is requested. */
        void onStorageAccessWebsiteDetailsRequested(WebsitePreference website);
    }

    WebsitePreference(
            Context context,
            SiteSettingsDelegate siteSettingsClient,
            Website site,
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

    public void setRefreshZoomsListFunction(Runnable refreshZoomsListCallback) {
        mRefreshZoomsListFunction = refreshZoomsListCallback;
    }

    public void setStorageAccessSettingsPageListener(
            OnStorageAccessWebsiteDetailsRequested storageAccessSettingsPageListener) {
        mStorageAccessSettingsPageListener = storageAccessSettingsPageListener;
    }

    public void putSiteIntoExtras(String key) {
        getExtras().putSerializable(key, mSite);
    }

    public void putSiteAddressIntoExtras(String key) {
        getExtras().putSerializable(key, mSite.getAddress());
    }

    /** Return the Website this object is representing. */
    public Website site() {
        return mSite;
    }

    /** Returns the url of the site to fetch a favicon for. */
    private GURL faviconUrl() {
        String origin = mSite.getMainAddress().getOrigin();
        GURL uri =
                new GURL(
                        origin.contains(WebsiteAddress.ANY_SUBDOMAIN_PATTERN)
                                ? origin.replace(WebsiteAddress.ANY_SUBDOMAIN_PATTERN, "")
                                : origin);
        return UrlUtilities.clearPort(uri);
    }

    /** @return if a |mCategory| has a sub page to show the |mSite| permissions. */
    private boolean hasSubPage() {
        int type = mCategory.getContentSettingsType();

        if (!Website.isEmbeddedPermission(type)) {
            return false;
        }

        int numberSites = mSite.getEmbeddedContentSettings(type).size();
        return numberSites != 1 || !mSite.isEmbargoed(type);
    }

    protected String buildTitle() {
        if (mCategory.getType() == SiteSettingsCategory.Type.STORAGE_ACCESS) {
            return mSite.getTitleForPreferenceRow();
        }

        return mSite.getTitle();
    }

    protected String buildExpirationSummary(ContentSettingException exception) {
        assert exception != null && exception.hasExpiration();
        var expirationInDays = exception.getExpirationInDays();
        return expirationInDays == 0
                ? getContext().getString(R.string.site_settings_expires_today_label)
                : getContext()
                        .getResources()
                        .getQuantityString(
                                R.plurals.site_settings_expires_label,
                                expirationInDays,
                                expirationInDays);
    }

    protected String buildSummary() {
        if (mSiteSettingsDelegate.isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                && mSiteSettingsDelegate.isRelatedWebsiteSetsDataAccessEnabled()
                && mSite.getRWSCookieInfo() != null) {
            var rwsInfo = mSite.getRWSCookieInfo();
            return getContext()
                    .getResources()
                    .getQuantityString(
                            R.plurals.allsites_rws_list_summary,
                            rwsInfo.getMembersCount(),
                            Integer.toString(rwsInfo.getMembersCount()),
                            rwsInfo.getOwner());
        }

        if (hasSubPage()) {
            int numberSites =
                    mSite.getEmbeddedContentSettings(mCategory.getContentSettingsType()).size();
            return getContext()
                    .getResources()
                    .getQuantityString(
                            R.plurals.number_sites, numberSites, Integer.toString(numberSites));
        }

        if (mSite.getEmbedder() == null) {
            if (mSite.isEmbargoed(mCategory.getContentSettingsType())) {
                return getContext().getString(R.string.automatically_blocked);
            }

            if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE
                    && mSite.getAddress().getIsAnySubdomainPattern()) {
                return getContext()
                        .getString(
                                R.string.website_settings_domain_exception_label,
                                mSite.getAddress().getHost());
            }

            return null;
        }

        if (mCategory.getType() == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            var exception = mSite.getContentSettingException(ContentSettingsType.COOKIES);
            if (exception != null && exception.hasExpiration()) {
                return buildExpirationSummary(exception);
            }
            return null;
        }

        String embedderTitle = mSite.getEmbedder().getTitle();
        if (mSite.representsThirdPartiesOnSite()
                || (embedderTitle != null
                        && (embedderTitle.isEmpty() || embedderTitle.equals(SITE_WILDCARD)))) {
            return null;
        }

        // TODO(crbug.com/40071127): Check if on Android there is a possibility of other exceptions
        // being scoped to an embedder.
        return getContext()
                .getString(R.string.website_settings_embedded_on, mSite.getEmbedder().getTitle());
    }

    protected void maybeSetImageView() {
        if (mCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
            // Create and set the delete button for this preference.
            setImageView(
                    R.drawable.ic_delete_white_24dp,
                    getContext()
                            .getResources()
                            .getString(
                                    R.string.site_settings_delete_zoom_level_content_description,
                                    buildTitle()),
                    (OnClickListener)
                            view -> {
                                SiteSettingsUtil.resetZoomLevel(
                                        mSite, mSiteSettingsDelegate.getBrowserContextHandle());
                                mRefreshZoomsListFunction.run();
                            });
            setImageViewEnabled(true);
            setImagePadding(25, 0, 0, 0);
            return;
        }

        if (hasSubPage()) {
            setImageView(
                    R.drawable.ic_expand_more_horizontal_black_24dp,
                    getContext()
                            .getResources()
                            .getString(
                                    R.string.webstorage_delete_data_content_description,
                                    buildTitle()),
                    (OnClickListener)
                            view -> {
                                mStorageAccessSettingsPageListener
                                        .onStorageAccessWebsiteDetailsRequested(this);
                            });
            setImageViewEnabled(true);
            setImagePadding(25, 0, 0, 0);
            return;
        }
    }

    protected void refresh() {
        setTitle(buildTitle());
        maybeSetImageView();
        setSummary(buildSummary());
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
                usageText.setVisibility(View.VISIBLE);
            }
        }
        if (mCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
            TextView summaryText = (TextView) holder.findViewById(android.R.id.summary);
            long readableZoomLevel =
                    Math.round(
                            100
                                    * PageZoomUtils.convertZoomFactorToZoomLevel(
                                            mSite.getZoomFactor()));
            summaryText.setText(
                    getContext().getString(R.string.page_zoom_level, readableZoomLevel));
            summaryText.setVisibility(View.VISIBLE);
            setViewClickable(false);
        }

        // Manually apply ListItemStartIcon style to draw the outer circle in the right size.
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched && faviconUrl().isValid()) {
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

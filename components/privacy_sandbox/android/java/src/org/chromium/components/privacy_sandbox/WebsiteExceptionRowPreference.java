// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/** Represents a row element for the 3PCD exceptions site list. */
public class WebsiteExceptionRowPreference extends ChromeImageViewPreference {
    /** Interface for the callback when the exception is deleted. */
    public interface WebsiteExceptionDeletedCallback {
        void refreshBlockingExceptions();
    }

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetchInProgress;

    private Website mSite;

    private TrackingProtectionDelegate mDelegate;

    private Context mContext;

    private WebsiteExceptionDeletedCallback mCallback;

    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";

    WebsiteExceptionRowPreference(
            Context context,
            Website site,
            TrackingProtectionDelegate delegate,
            WebsiteExceptionDeletedCallback callback) {
        super(context);
        mSite = site;
        mFaviconFetchInProgress = false;
        mDelegate = delegate;
        mContext = context;
        mCallback = callback;

        setTitle(site.getTitle());
        var exception = mSite.getContentSettingException(ContentSettingsType.COOKIES);
        if (exception != null && exception.hasExpiration()) {
            var expirationInDays = exception.getExpirationInDays();
            setSummary(
                    (expirationInDays == 0)
                            ? getContext()
                                    .getString(R.string.tracking_protection_expires_today_label)
                            : getContext()
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.tracking_protection_expires_label,
                                            expirationInDays,
                                            expirationInDays));
        } else {
            setSummary(getContext().getString(R.string.tracking_protection_never_expires_label));
        }
        setImageView(
                R.drawable.ic_delete_white_24dp,
                getContext()
                        .getString(R.string.tracking_protection_delete_site_label, site.getTitle()),
                (View view) -> {
                    deleteException();
                });
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Manually apply ListItemStartIcon style to draw the outer circle in the right size.
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetchInProgress && faviconUrl().isValid()) {
            // Start the favicon fetching. Will respond in onFaviconAvailable.
            mDelegate
                    .getSiteSettingsDelegate(mContext)
                    .getFaviconImageForURL(faviconUrl(), this::onFaviconAvailable);
            mFaviconFetchInProgress = true;
        }
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }

    /** Returns the url of the site to fetch a favicon for. */
    private GURL faviconUrl() {
        String origin = mSite.getMainAddress().getOrigin();
        GURL uri =
                new GURL(
                        origin.contains(ANY_SUBDOMAIN_PATTERN)
                                ? origin.replace(ANY_SUBDOMAIN_PATTERN, "")
                                : origin);
        return UrlUtilities.clearPort(uri);
    }

    private void deleteException() {
        mSite.setContentSetting(
                mDelegate.getBrowserContext(),
                ContentSettingsType.COOKIES,
                ContentSettingValues.DEFAULT);
        mCallback.refreshBlockingExceptions();
    }
}

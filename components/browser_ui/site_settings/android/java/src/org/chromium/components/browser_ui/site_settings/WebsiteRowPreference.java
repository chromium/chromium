// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.CallbackUtils;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Used by {@link AllSiteSettings} to display a row for a group of sites or a single site. */
public class WebsiteRowPreference extends ChromeImageViewPreference {
    private final SiteSettingsDelegate mSiteSettingsDelegate;
    private final WebsiteEntry mSiteEntry;

    private static final String HTTP = "http";

    // Whether the favicon has been fetched already.
    private boolean mFaviconFetched;

    private Dialog mConfirmationDialog;

    private LayoutInflater mLayoutInflater;

    private Runnable mOnDeleteCallback;

    WebsiteRowPreference(
            Context context,
            SiteSettingsDelegate siteSettingsDelegate,
            WebsiteEntry siteEntry,
            LayoutInflater layoutInflater) {
        super(context);
        mSiteSettingsDelegate = siteSettingsDelegate;
        mSiteEntry = siteEntry;
        mLayoutInflater = layoutInflater;
        // Initialize with an empty callback.
        mOnDeleteCallback = CallbackUtils.emptyRunnable();

        // To make sure the layout stays stable throughout, we assign a
        // transparent drawable as the icon initially. This is so that
        // we can fetch the favicon in the background and not have to worry
        // about the title appearing to jump (http://crbug.com/453626) when the
        // favicon becomes available.
        setIcon(new ColorDrawable(Color.TRANSPARENT));
        setTitle(mSiteEntry.getTitleForPreferenceRow());
        setImageView(
                R.drawable.ic_delete_white_24dp,
                context.getResources()
                        .getString(
                                R.string.webstorage_delete_data_content_description,
                                mSiteEntry.getTitleForPreferenceRow()),
                (View view) -> {
                    displayResetDialog();
                });
        updateSummary();
    }

    /**
     * Handles the click on the row by opening the appropriate SettingsFragment.
     * @param args the result of getArguments()
     * @param fromGrouped whether this is invoked from GroupedWebsitesSettings
     */
    @SuppressWarnings("WrongConstant")
    public void handleClick(Bundle args, boolean fromGrouped) {
        getExtras()
                .putSerializable(
                        mSiteEntry instanceof Website
                                ? SingleWebsiteSettings.EXTRA_SITE
                                : GroupedWebsitesSettings.EXTRA_GROUP,
                        mSiteEntry);
        if (fromGrouped) {
            getExtras().putBoolean(SingleWebsiteSettings.EXTRA_FROM_GROUPED, true);
        }
        setFragment(
                mSiteEntry instanceof Website
                        ? SingleWebsiteSettings.class.getName()
                        : GroupedWebsitesSettings.class.getName());
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

    public void setOnDeleteCallback(Runnable callback) {
        mOnDeleteCallback = callback;
    }

    private void displayResetDialog() {
        View dialogView = mLayoutInflater.inflate(R.layout.clear_reset_dialog, null);
        TextView mainMessage = dialogView.findViewById(R.id.main_message);
        mainMessage.setText(R.string.website_reset_confirmation);
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        signedOutText.setText(R.string.webstorage_clear_data_dialog_sign_out_message);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        offlineText.setText(R.string.webstorage_delete_data_dialog_offline_message);
        // TODO(crbug.com/40231223): Refactor and combine this with the ClearWebsiteStorageDialog
        // code.
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(dialogView)
                        .setTitle(R.string.website_reset_confirmation_title)
                        .setPositiveButton(
                                R.string.website_reset,
                                (dialog, which) -> {
                                    resetEntry();
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    @VisibleForTesting
    void resetEntry() {
        if (mSiteEntry instanceof Website) {
            SiteDataCleaner.resetPermissions(
                    mSiteSettingsDelegate.getBrowserContextHandle(), (Website) mSiteEntry);
            SiteDataCleaner.clearData(
                    mSiteSettingsDelegate, (Website) mSiteEntry, mOnDeleteCallback);
        } else {
            SiteDataCleaner.resetPermissions(
                    mSiteSettingsDelegate.getBrowserContextHandle(), (WebsiteGroup) mSiteEntry);
            SiteDataCleaner.clearData(
                    mSiteSettingsDelegate, (WebsiteGroup) mSiteEntry, mOnDeleteCallback);
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
            String cookie_str =
                    getContext()
                            .getResources()
                            .getQuantityString(R.plurals.cookies_count, cookies, cookies);
            if (summary.isEmpty()) {
                summary = cookie_str;
            } else {
                summary =
                        getContext()
                                .getString(R.string.summary_with_one_bullet, cookie_str, summary);
            }
        }

        // When a single HTTP origin is represented, mark it as such.
        if (isSiteEntryASingleHttpOrigin()) {
            if (summary.isEmpty()) {
                summary = HTTP;
            } else {
                summary = getContext().getString(R.string.summary_with_one_bullet, HTTP, summary);
            }
        }

        if (mSiteSettingsDelegate.shouldShowPrivacySandboxRwsUi()) {
            if (mSiteEntry.isPartOfRws()) {
                String rwsSummary =
                        getContext()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.rws_summary,
                                        mSiteEntry.getRwsSize(),
                                        Integer.toString(mSiteEntry.getRwsSize()),
                                        mSiteEntry.getRwsOwner());
                if (summary.isEmpty()) {
                    summary = rwsSummary;
                } else {
                    summary =
                            getContext()
                                    .getString(
                                            R.string.summary_with_one_bullet, summary, rwsSummary);
                }
            }
        }

        if (!summary.isEmpty()) {
            setSummary(summary);
        }
    }
}

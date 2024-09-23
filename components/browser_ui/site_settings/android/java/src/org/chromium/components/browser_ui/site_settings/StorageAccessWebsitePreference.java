// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.view.View.OnClickListener;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.List;

/**
 * A preference for embedded Storage Access permission that displays the embedder website's favicon
 * and URL, and an icon on the RHS to reset the permission. See {@link WebsitePreference} for more
 * details on how this preference can be used.
 */
class StorageAccessWebsitePreference extends WebsitePreference {

    private final OnStorageAccessWebsiteReset mOnStorageAccessWebsiteResetListener;

    /** Used to notify storage access embedded website permission reset requests. */
    public interface OnStorageAccessWebsiteReset {
        /** Notify that the embedded website permission has been reset. */
        void onStorageAccessWebsiteReset(StorageAccessWebsitePreference preference);
    }

    StorageAccessWebsitePreference(
            Context context,
            SiteSettingsDelegate siteSettingsClient,
            Website site,
            OnStorageAccessWebsiteReset onStorageAccessWebsiteResetListener) {
        super(
                context,
                siteSettingsClient,
                site,
                SiteSettingsCategory.createFromType(
                        siteSettingsClient.getBrowserContextHandle(),
                        SiteSettingsCategory.Type.STORAGE_ACCESS));

        mOnStorageAccessWebsiteResetListener = onStorageAccessWebsiteResetListener;
    }

    @Override
    protected String buildTitle() {
        return mSite.getTitleForEmbeddedPreferenceRow();
    }

    @Override
    protected String buildSummary() {

        List<ContentSettingException> exceptions =
                mSite.getEmbeddedContentSettings(ContentSettingsType.STORAGE_ACCESS);
        assert exceptions.size() == 1;

        ContentSettingException exception = exceptions.get(0);
        if (exception.isEmbargoed()) {
            return getContext().getString(R.string.automatically_blocked);
        }

        if (exception != null && exception.hasExpiration()) {
            return buildExpirationSummary(exception);
        }

        return null;
    }

    @Override
    protected void maybeSetImageView() {
        setImageView(
                R.drawable.ic_delete_white_24dp,
                getContext()
                        .getResources()
                        .getString(
                                R.string.webstorage_delete_data_content_description, buildTitle()),
                (OnClickListener)
                        view -> {
                            mSite.setContentSetting(
                                    mSiteSettingsDelegate.getBrowserContextHandle(),
                                    mCategory.getContentSettingsType(),
                                    ContentSettingValues.DEFAULT);
                            mOnStorageAccessWebsiteResetListener.onStorageAccessWebsiteReset(this);
                        });
        setImageViewEnabled(true);
        setImagePadding(25, 0, 0, 0);
    }

    @Override
    protected void refresh() {
        assert mCategory.getType() == SiteSettingsCategory.Type.STORAGE_ACCESS;
        setSelectable(false);
        super.refresh();
    }
}

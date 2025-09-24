// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;

/** Subpage fragment showing durable location permission options of a site. */
@NullMarked
public class LocationPermissionSubpageSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage {
    public static final String RADIO_BUTTON_GROUP_KEY = "radio_button_group";
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        mPageTitle.set(getContext().getString(R.string.website_settings_device_location));

        // Remove this Preference if it gets restored without a valid SiteSettingsDelegate. This
        // can happen e.g. when it is included in PageInfo.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }

        Website site = (Website) getArguments().getSerializable(SingleWebsiteSettings.EXTRA_SITE);
        assert site != null;

        PermissionInfo permissionInfo =
                site.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        assert permissionInfo != null;
        assert permissionInfo.getSessionModel() == SessionModel.DURABLE;

        SettingsUtils.addPreferencesFromResource(this, R.xml.location_permission_settings);

        LocationPermissionOptionsPreference radioPreference =
                findPreference(RADIO_BUTTON_GROUP_KEY);
        assert radioPreference != null;
        radioPreference.initialize(getSiteSettingsDelegate().getBrowserContextHandle(), site);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}

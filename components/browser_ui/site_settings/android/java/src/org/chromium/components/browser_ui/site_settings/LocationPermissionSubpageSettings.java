// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;

import java.util.Collection;

/** Subpage fragment showing durable location permission options of a site. */
@NullMarked
public class LocationPermissionSubpageSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage {
    public static final String RADIO_BUTTON_GROUP_KEY = "radio_button_group";
    public static final String PREF_OS_PERMISSIONS_WARNING = "os_permissions_warning";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private @MonotonicNonNull Website mSite;

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
        WebsiteAddress address =
                (WebsiteAddress)
                        getArguments().getSerializable(SingleWebsiteSettings.EXTRA_SITE_ADDRESS);

        if (site != null && address == null) {
            mSite = site;
            setUpPreferences();
        } else if (address != null && site == null) {
            WebsitePermissionsFetcher fetcher =
                    new WebsitePermissionsFetcher(getSiteSettingsDelegate());
            fetcher.fetchAllPreferences(
                    (Collection<Website> sites) -> {
                        // This method may be called after the activity has been destroyed.
                        // In that case, bail out.
                        if (getActivity() == null) return;

                        mSite =
                                SingleWebsiteSettings
                                        .mergePermissionAndStorageInfoForTopLevelOrigin(
                                                address, sites);
                        setUpPreferences();
                    });
        } else {
            assert false : "Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.";
        }
    }

    private void setUpPreferences() {
        assumeNonNull(mSite);
        PermissionInfo permissionInfo =
                mSite.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        assert permissionInfo != null;
        assert permissionInfo.getSessionModel() == SessionModel.DURABLE;

        SettingsUtils.addPreferencesFromResource(this, R.xml.location_permission_settings);

        LocationPermissionOptionsPreference radioPreference =
                findPreference(RADIO_BUTTON_GROUP_KEY);
        assert radioPreference != null;
        radioPreference.initialize(
                getSiteSettingsDelegate().getBrowserContextHandle(), mSite, this);

        setUpOsWarningPreferences();
    }

    public void setUpOsWarningPreferences() {
        assumeNonNull(mSite);
        PermissionInfo info =
                mSite.getPermissionInfo(
                        SiteSettingsCategory.contentSettingsType(
                                SiteSettingsCategory.Type.DEVICE_LOCATION));
        assumeNonNull(info);
        GeolocationSetting permission =
                info.getGeolocationSetting(getSiteSettingsDelegate().getBrowserContextHandle());
        boolean isPreciseSelected = permission.mPrecise == ContentSetting.ALLOW;

        LocationCategory locationCategory =
                new LocationCategory(
                        getSiteSettingsDelegate().getBrowserContextHandle(), isPreciseSelected);

        LocationPermissionOptionsPreference radioPreference =
                findPreference(RADIO_BUTTON_GROUP_KEY);
        if (radioPreference != null) {
            if (isPreciseSelected && locationCategory.hasPreciseOnlyBlockedWarning(getContext())) {
                int summaryResId = R.string.website_settings_using_approximate_location_summary;
                String summary = getContext().getString(summaryResId);
                radioPreference.setPreciseSummary(summary);
            } else {
                radioPreference.setPreciseSummary("");
            }
        }

        if (!locationCategory.showPermissionBlockedMessage(getContext())) {
            Preference preference = findPreference(PREF_OS_PERMISSIONS_WARNING);
            if (preference != null) getPreferenceScreen().removePreference(preference);
            return;
        }

        Preference osWarning = findPreference(PREF_OS_PERMISSIONS_WARNING);
        if (osWarning == null) {
            osWarning = new Preference(getContext());
            osWarning.setKey(PREF_OS_PERMISSIONS_WARNING);
            // This should be at the end of the list.
            osWarning.setOrder(1000);
        }

        Preference osWarningExtra = new Preference(getContext());
        locationCategory.configureWarningPreferences(
                osWarning, osWarningExtra, getContext(), getSiteSettingsDelegate().getAppName());
        getPreferenceScreen().addPreference(osWarning);
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

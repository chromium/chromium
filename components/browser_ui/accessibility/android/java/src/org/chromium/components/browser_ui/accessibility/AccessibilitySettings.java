// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.base.UiAndroidFeatureList;

/** Fragment to keep track of all the accessibility related preferences. */
@NullMarked
public class AccessibilitySettings extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage,
                Preference.OnPreferenceChangeListener,
                CustomDividerFragment {
    public static final String PREF_PAGE_ZOOM_DEFAULT_ZOOM = "page_zoom_default_zoom";
    public static final String PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT =
            "page_zoom_include_os_adjustment";
    public static final String PREF_PAGE_ZOOM_ALWAYS_SHOW = "page_zoom_always_show";
    public static final String PREF_FORCE_ENABLE_ZOOM = "force_enable_zoom";
    public static final String PREF_READER_FOR_ACCESSIBILITY = "reader_for_accessibility";
    public static final String PREF_CAPTIONS = "captions";
    public static final String PREF_ZOOM_INFO = "zoom_info";
    public static final String PREF_IMAGE_DESCRIPTIONS = "image_descriptions";
    public static final String PREF_CARET_BROWSING = "caret_browsing";
    public static final String PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION =
            "touchpad_overscroll_history_navigation";

    private PageZoomPreference mPageZoomDefaultZoomPref;
    private ChromeSwitchPreference mPageZoomIncludeOSAdjustment;
    private ChromeSwitchPreference mPageZoomAlwaysShowPref;
    private ChromeSwitchPreference mForceEnableZoomPref;
    private ChromeSwitchPreference mCaretBrowsingPref;
    private ChromeSwitchPreference mJumpStartOmnibox;
    private AccessibilitySettingsDelegate mDelegate;
    private double mPageZoomLatestDefaultZoomPrefValue;
    private ChromeSwitchPreference mTouchpadOverscrollHistoryNavigationPref;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    public void setDelegate(AccessibilitySettingsDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mPageTitle.set(getString(R.string.prefs_accessibility));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.accessibility_preferences);

        // TODO(crbug.com/439911511): Add PageZoomPreference directly to the xml file instead.
        // Create the page zoom preference.
        if (mDelegate.shouldUseSlider()) {
            mPageZoomDefaultZoomPref = new PageZoomSliderPreference(getContext(), null);
        } else {
            mPageZoomDefaultZoomPref = new PageZoomSeekbarPreference(getContext(), null);
        }
        mPageZoomDefaultZoomPref.setKey(PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        mPageZoomDefaultZoomPref.setOrder(-1);
        getPreferenceScreen().addPreference(mPageZoomDefaultZoomPref);

        mPageZoomAlwaysShowPref = findPreference(PREF_PAGE_ZOOM_ALWAYS_SHOW);
        mPageZoomIncludeOSAdjustment = findPreference(PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);

        // Set the initial values for the page zoom settings, and set change listeners.
        mPageZoomDefaultZoomPref.setInitialValue(
                PageZoomUtils.getDefaultZoomAsBarValue(mDelegate.getBrowserContextHandle()));
        mPageZoomDefaultZoomPref.setOnPreferenceChangeListener(this);
        mPageZoomAlwaysShowPref.setChecked(PageZoomUtils.shouldShowZoomMenuItem());
        mPageZoomAlwaysShowPref.setOnPreferenceChangeListener(this);

        // When Smart Zoom feature is enabled, set the required delegate.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
            mPageZoomDefaultZoomPref.setTextSizeContrastDelegate(
                    mDelegate.getTextSizeContrastAccessibilityDelegate());
        }

        mForceEnableZoomPref = findPreference(PREF_FORCE_ENABLE_ZOOM);
        mForceEnableZoomPref.setOnPreferenceChangeListener(this);
        mForceEnableZoomPref.setChecked(
                mDelegate.getForceEnableZoomAccessibilityDelegate().getValue());

        mJumpStartOmnibox = findPreference(OmniboxFeatures.KEY_JUMP_START_OMNIBOX);
        mJumpStartOmnibox.setOnPreferenceChangeListener(this);
        mJumpStartOmnibox.setChecked(OmniboxFeatures.isJumpStartOmniboxEnabled());
        mJumpStartOmnibox.setVisible(OmniboxFeatures.sJumpStartOmnibox.isEnabled());

        ChromeSwitchPreference readerForAccessibilityPref =
                findPreference(PREF_READER_FOR_ACCESSIBILITY);
        readerForAccessibilityPref.setChecked(
                mDelegate.getReaderAccessibilityDelegate().getValue());
        readerForAccessibilityPref.setOnPreferenceChangeListener(this);

        Preference captions = findPreference(PREF_CAPTIONS);
        captions.setOnPreferenceClickListener(
                preference -> {
                    Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

                    // Open the activity in a new task because the back button on the caption
                    // settings page navigates to the previous settings page instead of Chrome.
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(intent);

                    return true;
                });

        Preference zoomInfo = findPreference(PREF_ZOOM_INFO);
        zoomInfo.setVisible(true);
        zoomInfo.setOnPreferenceClickListener(
                preference -> {
                    Bundle initialArguments = new Bundle();
                    initialArguments.putString(
                            SingleCategorySettings.EXTRA_CATEGORY,
                            SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ZOOM));
                    initialArguments.putString(
                            AllSiteSettings.EXTRA_TITLE,
                            getString(R.string.zoom_info_preference_title));
                    mDelegate
                            .getSiteSettingsNavigation()
                            .startSettings(
                                    ContextUtils.getApplicationContext(),
                                    AllSiteSettings.class,
                                    initialArguments,
                                    /* addToBackStack= */ true);
                    return true;
                });

        // When Accessibility Page Zoom v2 is enabled, show additional controls.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2)) {
            mPageZoomIncludeOSAdjustment.setVisible(true);
            mPageZoomIncludeOSAdjustment.setOnPreferenceChangeListener(this);
        } else {
            mPageZoomIncludeOSAdjustment.setVisible(false);
        }

        Preference imageDescriptionsPreference = findPreference(PREF_IMAGE_DESCRIPTIONS);
        imageDescriptionsPreference.setVisible(mDelegate.shouldShowImageDescriptionsSetting());

        // Caret Browsing Settings
        mCaretBrowsingPref = findPreference(PREF_CARET_BROWSING);

        if (ContentFeatureList.sAndroidCaretBrowsing.isEnabled()) {
            mCaretBrowsingPref.setChecked(mDelegate.isCaretBrowsingEnabled());
            mCaretBrowsingPref.setOnPreferenceChangeListener(this);
        } else {
            mCaretBrowsingPref.setVisible(false);
        }

        // Touchpad swipe-to-navigate settings.
        mTouchpadOverscrollHistoryNavigationPref =
                findPreference(PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION);
        if (UiAndroidFeatureList.sAndroidTouchpadOverscrollHistoryNavigation.isEnabled()) {
            mTouchpadOverscrollHistoryNavigationPref.setVisible(true);
            mTouchpadOverscrollHistoryNavigationPref.setOnPreferenceChangeListener(this);
            mTouchpadOverscrollHistoryNavigationPref.setChecked(
                    mDelegate
                            .getTouchpadOverscrollHistoryNavigationAccessibilityDelegate()
                            .getValue());
        } else {
            mTouchpadOverscrollHistoryNavigationPref.setVisible(false);
        }
    }

    @Override
    public void onStop() {
        // Ensure that the user has set a default zoom value during this session.
        if (mPageZoomLatestDefaultZoomPrefValue != 0.0) {
            PageZoomUma.logSettingsDefaultZoomLevelChangedHistogram();
            PageZoomUma.logSettingsDefaultZoomLevelValueHistogram(
                    mPageZoomLatestDefaultZoomPrefValue);
        }

        super.onStop();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_FORCE_ENABLE_ZOOM.equals(preference.getKey())) {
            mDelegate.getForceEnableZoomAccessibilityDelegate().setValue((Boolean) newValue);
        } else if (PREF_READER_FOR_ACCESSIBILITY.equals(preference.getKey())) {
            boolean readerModeEnabled = (Boolean) newValue;
            mDelegate.getReaderAccessibilityDelegate().setValue(readerModeEnabled);
            RecordHistogram.recordBooleanHistogram(
                    "DomDistiller.Android.ReaderModeEnabledInAccessibilitySettings",
                    readerModeEnabled);
        } else if (PREF_PAGE_ZOOM_DEFAULT_ZOOM.equals(preference.getKey())) {
            mPageZoomLatestDefaultZoomPrefValue =
                    PageZoomUtils.convertBarValueToZoomLevel((Integer) newValue);
            PageZoomUtils.setDefaultZoomByBarValue(
                    mDelegate.getBrowserContextHandle(), (Integer) newValue);
        } else if (PREF_PAGE_ZOOM_ALWAYS_SHOW.equals(preference.getKey())) {
            PageZoomUtils.setShouldAlwaysShowZoomMenuItem((Boolean) newValue);
        } else if (PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT.equals(preference.getKey())) {
            // TODO(mschillaci): Implement the override behavior for OS level.
        } else if (OmniboxFeatures.KEY_JUMP_START_OMNIBOX.equals(preference.getKey())) {
            OmniboxFeatures.setJumpStartOmniboxEnabled((Boolean) newValue);
        } else if (PREF_CARET_BROWSING.equals(preference.getKey())) {
            mDelegate.setCaretBrowsingEnabled((Boolean) newValue);
        } else if (PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION.equals(preference.getKey())) {
            mDelegate
                    .getTouchpadOverscrollHistoryNavigationAccessibilityDelegate()
                    .setValue((Boolean) newValue);
        }
        return true;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "accessibility";
    }

    @Override
    public boolean hasDivider() {
        return false;
    }
}

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
import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate.BooleanPreferenceDelegate;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

/**
 * Fragment to keep track of all the accessibility related preferences.
 */
public class AccessibilitySettings extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener, CustomDividerFragment {
    public static final String PREF_TEXT_SCALE = "text_scale";
    public static final String PREF_PAGE_ZOOM_DEFAULT_ZOOM = "page_zoom_default_zoom";
    public static final String PREF_PAGE_ZOOM_ALWAYS_SHOW = "page_zoom_always_show";
    public static final String PREF_FORCE_ENABLE_ZOOM = "force_enable_zoom";
    public static final String PREF_READER_FOR_ACCESSIBILITY = "reader_for_accessibility";
    public static final String PREF_CAPTIONS = "captions";
    public static final String PREF_ZOOM_INFO = "zoom_info";

    private TextScalePreference mTextScalePref;
    private PageZoomPreference mPageZoomDefaultZoomPref;
    private ChromeSwitchPreference mPageZoomAlwaysShowPref;
    private ChromeSwitchPreference mForceEnableZoomPref;
    private boolean mRecordFontSizeChangeOnStop;
    private AccessibilitySettingsDelegate mDelegate;
    private BooleanPreferenceDelegate mReaderForAccessibilityDelegate;
    private double mPageZoomLatestDefaultZoomPrefValue;

    private FontSizePrefs mFontSizePrefs;
    private FontSizePrefsObserver mFontSizePrefsObserver = new FontSizePrefsObserver() {
        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            mTextScalePref.updateFontScaleFactors(fontScaleFactor, userFontScaleFactor, true);
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            mForceEnableZoomPref.setChecked(enabled);
        }
    };

    public void setDelegate(AccessibilitySettingsDelegate delegate) {
        mDelegate = delegate;
        mFontSizePrefs = FontSizePrefs.getInstance(delegate.getBrowserContextHandle());
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        getActivity().setTitle(
                ContextUtils.getApplicationContext().getString(R.string.prefs_accessibility));
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.accessibility_preferences);

        mTextScalePref = (TextScalePreference) findPreference(PREF_TEXT_SCALE);
        mPageZoomDefaultZoomPref = (PageZoomPreference) findPreference(PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        mPageZoomAlwaysShowPref =
                (ChromeSwitchPreference) findPreference(PREF_PAGE_ZOOM_ALWAYS_SHOW);

        if (mDelegate.showPageZoomSettingsUI()) {
            mTextScalePref.setVisible(false);
            // Set the initial values for the page zoom settings, and set change listeners.
            mPageZoomDefaultZoomPref.setInitialValue(PageZoomUtils.getDefaultZoomAsSeekBarValue(
                    mDelegate.getBrowserContextHandle()));
            mPageZoomDefaultZoomPref.setOnPreferenceChangeListener(this);
            mPageZoomAlwaysShowPref.setChecked(PageZoomUtils.shouldShowZoomMenuItem());
            mPageZoomAlwaysShowPref.setOnPreferenceChangeListener(this);
            // When Smart Zoom feature is enabled, set the required delegate.
            if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
                mPageZoomDefaultZoomPref.setTextSizeContrastDelegate(
                        mDelegate.getTextSizeContrastAccessibilityDelegate());
            }
        } else {
            mPageZoomDefaultZoomPref.setVisible(false);
            mPageZoomAlwaysShowPref.setVisible(false);
            mTextScalePref.setOnPreferenceChangeListener(this);
            mTextScalePref.updateFontScaleFactors(mFontSizePrefs.getFontScaleFactor(),
                    mFontSizePrefs.getUserFontScaleFactor(), false);
        }

        mForceEnableZoomPref = (ChromeSwitchPreference) findPreference(PREF_FORCE_ENABLE_ZOOM);
        mForceEnableZoomPref.setOnPreferenceChangeListener(this);
        mForceEnableZoomPref.setChecked(mFontSizePrefs.getForceEnableZoom());

        ChromeSwitchPreference readerForAccessibilityPref =
                (ChromeSwitchPreference) findPreference(PREF_READER_FOR_ACCESSIBILITY);
        mReaderForAccessibilityDelegate = mDelegate.getReaderForAccessibilityDelegate();
        if (mReaderForAccessibilityDelegate != null) {
            readerForAccessibilityPref.setChecked(mReaderForAccessibilityDelegate.isEnabled());
            readerForAccessibilityPref.setOnPreferenceChangeListener(this);
        } else {
            getPreferenceScreen().removePreference(readerForAccessibilityPref);
        }

        Preference captions = findPreference(PREF_CAPTIONS);
        captions.setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

            // Open the activity in a new task because the back button on the caption
            // settings page navigates to the previous settings page instead of Chrome.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);

            return true;
        });

        Preference zoomInfo = findPreference(PREF_ZOOM_INFO);
        if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
            zoomInfo.setVisible(true);
            zoomInfo.setOnPreferenceClickListener(preference -> {
                mDelegate.launchSiteSettingsZoomActivity(getContext());
                return true;
            });
        } else {
            zoomInfo.setVisible(false);
        }

        mDelegate.addExtraPreferences(this);
    }

    @Override
    public void onStart() {
        super.onStart();
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
    }

    @Override
    public void onStop() {
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
        if (mRecordFontSizeChangeOnStop) {
            mFontSizePrefs.recordUserFontPrefChange();
            mRecordFontSizeChangeOnStop = false;
        }

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
        if (PREF_TEXT_SCALE.equals(preference.getKey())) {
            mRecordFontSizeChangeOnStop = true;
            mFontSizePrefs.setUserFontScaleFactor((Float) newValue);
        } else if (PREF_FORCE_ENABLE_ZOOM.equals(preference.getKey())) {
            mFontSizePrefs.setForceEnableZoomFromUser((Boolean) newValue);
        } else if (PREF_READER_FOR_ACCESSIBILITY.equals(preference.getKey())) {
            if (mReaderForAccessibilityDelegate != null) {
                mReaderForAccessibilityDelegate.setEnabled((Boolean) newValue);
            }
        } else if (PREF_PAGE_ZOOM_DEFAULT_ZOOM.equals(preference.getKey())) {
            mPageZoomLatestDefaultZoomPrefValue =
                    PageZoomUtils.convertSeekBarValueToZoomLevel((Integer) newValue);
            PageZoomUtils.setDefaultZoomBySeekBarValue(
                    mDelegate.getBrowserContextHandle(), (Integer) newValue);
        } else if (PREF_PAGE_ZOOM_ALWAYS_SHOW.equals(preference.getKey())) {
            PageZoomUtils.setShouldAlwaysShowZoomMenuItem((Boolean) newValue);
        }
        return true;
    }
}

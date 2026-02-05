// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.PreferenceParser;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
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
    public static final String PREF_READER_ENABLE_LINKS = "reader_enable_links";
    public static final String PREF_CAPTIONS = "captions";
    public static final String PREF_ZOOM_INFO = "zoom_info";
    public static final String PREF_IMAGE_DESCRIPTIONS = "image_descriptions";
    public static final String PREF_CARET_BROWSING = "caret_browsing";
    public static final String PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION =
            "touchpad_overscroll_history_navigation";

    @VisibleForTesting
    final DistilledPagePrefs.Observer mDistilledPagePrefsObserver =
            new DistilledPagePrefs.Observer() {
                @Override
                public void onChangeFontFamily(int font) {}

                @Override
                public void onChangeTheme(int theme) {}

                @Override
                public void onChangeFontScaling(float scaling) {}

                @Override
                public void onChangeLinksEnabled(boolean enabled) {
                    ChromeSwitchPreference readerEnableLinks =
                            findPreference(PREF_READER_ENABLE_LINKS);
                    readerEnableLinks.setChecked(enabled);
                }
            };

    private PageZoomPreference mPageZoomDefaultZoomPref;
    private ChromeSwitchPreference mPageZoomIncludeOSAdjustment;
    private ChromeSwitchPreference mPageZoomAlwaysShowPref;
    private ChromeSwitchPreference mForceEnableZoomPref;
    private ChromeSwitchPreference mCaretBrowsingPref;
    private ChromeSwitchPreference mJumpStartOmnibox;
    private AccessibilitySettingsDelegate mDelegate;
    private double mPageZoomLatestDefaultZoomPrefValue;
    private ChromeSwitchPreference mTouchpadOverscrollHistoryNavigationPref;
    private @Nullable DistilledPagePrefs mDistilledPagePrefs;

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    public void setDelegate(AccessibilitySettingsDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mPageTitle.set(getString(R.string.prefs_accessibility));
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
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
        if (PageZoomPreference.shouldShowTextSizeContrastSetting()) {
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
        mJumpStartOmnibox.setVisible(shouldShowJumpStartOmniboxPref());

        ChromeSwitchPreference readerForAccessibilityPref =
                findPreference(PREF_READER_FOR_ACCESSIBILITY);
        readerForAccessibilityPref.setChecked(
                mDelegate.getReaderAccessibilityDelegate().getValue());
        readerForAccessibilityPref.setOnPreferenceChangeListener(this);

        boolean toggleLinksEnabled = shouldShowReadingModeToggleLinks();
        ChromeSwitchPreference readerEnableLinks = findPreference(PREF_READER_ENABLE_LINKS);
        readerEnableLinks.setVisible(toggleLinksEnabled);
        if (toggleLinksEnabled) {
            mDistilledPagePrefs = mDelegate.getDistilledPagePrefs();
            mDistilledPagePrefs.addObserver(mDistilledPagePrefsObserver);
            mDistilledPagePrefsObserver.onChangeLinksEnabled(mDistilledPagePrefs.getLinksEnabled());
            readerEnableLinks.setOnPreferenceChangeListener(this);
        }

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
        if (shouldShowPageZoomV2()) {
            mPageZoomIncludeOSAdjustment.setVisible(true);
            mPageZoomIncludeOSAdjustment.setOnPreferenceChangeListener(this);
        } else {
            mPageZoomIncludeOSAdjustment.setVisible(false);
        }

        Preference imageDescriptionsPreference = findPreference(PREF_IMAGE_DESCRIPTIONS);
        imageDescriptionsPreference.setVisible(shouldShowImageDescriptionsPref(mDelegate));

        // Caret Browsing Settings
        mCaretBrowsingPref = findPreference(PREF_CARET_BROWSING);

        if (shouldShowCaretBrowsingPref()) {
            mCaretBrowsingPref.setChecked(mDelegate.isCaretBrowsingEnabled());
            mCaretBrowsingPref.setOnPreferenceChangeListener(this);
        } else {
            mCaretBrowsingPref.setVisible(false);
        }

        // Touchpad swipe-to-navigate settings.
        mTouchpadOverscrollHistoryNavigationPref =
                findPreference(PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION);
        if (shouldShowTouchpadOverscrollHistoryNavigationPref()) {
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
    public void onDestroy() {
        if (mDistilledPagePrefs != null) {
            mDistilledPagePrefs.removeObserver(mDistilledPagePrefsObserver);
        }

        super.onDestroy();
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
        } else if (PREF_READER_ENABLE_LINKS.equals(preference.getKey())) {
            // This preference is only registered if mDistilledPagePrefs is non-null.
            assert mDistilledPagePrefs != null;
            boolean readerEnableLinks = (Boolean) newValue;
            mDistilledPagePrefs.setLinksEnabled(readerEnableLinks);
            RecordHistogram.recordBooleanHistogram(
                    "DomDistiller.Android.ReaderModeEnableLinksInAccessibilitySettings",
                    readerEnableLinks);
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

    private static boolean shouldShowJumpStartOmniboxPref() {
        return OmniboxFeatures.sJumpStartOmnibox.isEnabled();
    }

    private static boolean shouldShowPageZoomV2() {
        return ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2);
    }

    private static boolean shouldShowCaretBrowsingPref() {
        return ContentFeatureList.sAndroidCaretBrowsing.isEnabled();
    }

    private static boolean shouldShowTouchpadOverscrollHistoryNavigationPref() {
        return UiAndroidFeatureList.sAndroidTouchpadOverscrollHistoryNavigation.isEnabled();
    }

    private static boolean shouldShowImageDescriptionsPref(AccessibilitySettingsDelegate delegate) {
        return delegate.shouldShowImageDescriptionsSetting();
    }

    private static boolean shouldShowReadingModeToggleLinks() {
        return DomDistillerFeatures.sReaderModeToggleLinks.isEnabled();
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    AccessibilitySettings.class.getName(), R.xml.accessibility_preferences);

    /**
     * Update dynamic preferences with an object of the interface {@link
     * AccessibilitySettingsDelegate}.
     *
     * <p>The implementation of the interface has dependencies outside //components, therefore
     * cannot be instantiated in BaseSearchIndexProvider#updateDynamicPreferences. This is handled
     * as an exception.
     */
    public static void updateDynamicPreferences(
            Context context, AccessibilitySettingsDelegate delegate, SettingsIndexData indexData) {
        String prefFragment = AccessibilitySettings.class.getName();
        int subViewPos = 0;
        addEntryForKey(
                indexData,
                prefFragment,
                PREF_PAGE_ZOOM_DEFAULT_ZOOM,
                PREF_PAGE_ZOOM_DEFAULT_ZOOM,
                subViewPos,
                R.string.page_zoom_title,
                R.string.page_zoom_summary);
        subViewPos += 1;
        // Default zoom/Text size contrast/Preview are under a single preference. Add 2 virtual
        // entries to index and make the latter 2 searchable.
        if (PageZoomPreference.shouldShowTextSizeContrastSetting()) {
            addEntryForKey(
                    indexData,
                    prefFragment,
                    "page_zoom_text_size_contrast",
                    PREF_PAGE_ZOOM_DEFAULT_ZOOM,
                    subViewPos,
                    R.string.text_size_contrast_title,
                    R.string.text_size_contrast_summary);
            subViewPos += 1;
        }
        addEntryForKey(
                indexData,
                prefFragment,
                "page_zoom_preview",
                PREF_PAGE_ZOOM_DEFAULT_ZOOM,
                subViewPos,
                R.string.page_zoom_preview_title,
                R.string.page_zoom_preview_text_summary);
        if (!shouldShowJumpStartOmniboxPref()) {
            indexData.removeEntryForKey(prefFragment, OmniboxFeatures.KEY_JUMP_START_OMNIBOX);
        }
        if (!shouldShowPageZoomV2()) {
            indexData.removeEntryForKey(prefFragment, PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);
        }
        if (!shouldShowImageDescriptionsPref(delegate)) {
            indexData.removeEntryForKey(prefFragment, PREF_IMAGE_DESCRIPTIONS);
        }
        if (!shouldShowCaretBrowsingPref()) {
            indexData.removeEntryForKey(prefFragment, PREF_CARET_BROWSING);
        }
        if (!shouldShowTouchpadOverscrollHistoryNavigationPref()) {
            indexData.removeEntryForKey(prefFragment, PREF_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION);
        }
        if (!shouldShowReadingModeToggleLinks()) {
            indexData.removeEntryForKey(prefFragment, PREF_READER_ENABLE_LINKS);
        }
    }

    private static void addEntryForKey(
            SettingsIndexData indexData,
            String parentFragment,
            String key,
            String highlightKey,
            int subViewPos,
            int titleId,
            int summaryId) {
        String id = PreferenceParser.createUniqueId(parentFragment, key);
        Context context = ContextUtils.getApplicationContext();
        String title = context.getString(titleId);
        indexData.addEntry(
                id,
                new SettingsIndexData.Entry.Builder(id, key, title, parentFragment)
                        .setSummary(context.getString(summaryId))
                        .setHighlightKey(highlightKey)
                        .setSubViewPos(subViewPos)
                        .build());
    }
}

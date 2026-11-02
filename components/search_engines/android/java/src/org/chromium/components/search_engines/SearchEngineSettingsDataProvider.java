// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.List;

/**
 * Java wrapper for the native SearchEngineSettingsDataProvider. Prepares search engine data for the
 * settings screens, and owns the once-per-session settings telemetry for those screens.
 *
 * <p>Owned by a settings screen ({@code SearchEngineAdapter} for the search engine list, {@code
 * SiteSearchSettings} for the site search page, which shares its instance with all of its sections)
 * and lives for the full lifetime of that screen. The underlying native profile services
 * (TemplateURLService, RegionalCapabilitiesService) outlive the settings UI.
 */
@JNINamespace("search_engines")
@NullMarked
public class SearchEngineSettingsDataProvider implements AutoCloseable {
    @JniType("::search_engines::SearchEngineSettingsDataProvider")
    interface NativeSearchEngineSettingsDataProvider extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeSearchEngineSettingsDataProvider> mNativeProvider;

    /* package */ SearchEngineSettingsDataProvider(
            JniUniquePtr<NativeSearchEngineSettingsDataProvider> nativeProvider) {
        mNativeProvider = nativeProvider;
    }

    @Override
    public void close() {
        ThreadUtils.assertOnUiThread();
        if (mNativeProvider == null) return;
        mNativeProvider.destroy();
        mNativeProvider = null;
    }

    public PrepopulatedAndRecentlyVisitedTemplateURLs
            getPrepopulatedAndRecentlyVisitedTemplateURLs() {
        ThreadUtils.assertOnUiThread();
        assert mNativeProvider != null;
        return SearchEngineSettingsDataProviderJni.get()
                .getPrepopulatedAndRecentlyVisitedTemplateURLs(mNativeProvider);
    }

    public List<TemplateUrl> getTemplateUrlsByCategory(@TemplateUrlCategory int category) {
        ThreadUtils.assertOnUiThread();
        assert mNativeProvider != null;
        return SearchEngineSettingsDataProviderJni.get()
                .getTemplateUrlsByCategory(mNativeProvider, category);
    }

    @SafeVarargs
    public final void maybeRecordSettingsPageLoadMetrics(
            List<TemplateUrl>... displayedEngineLists) {
        maybeRecordSettingsPageLoadMetrics(Arrays.asList(displayedEngineLists));
    }

    public void maybeRecordSettingsPageLoadMetrics(List<List<TemplateUrl>> displayedEngineLists) {
        ThreadUtils.assertOnUiThread();
        assert mNativeProvider != null;
        int totalSize = 0;
        for (List<TemplateUrl> list : displayedEngineLists) {
            totalSize += list.size();
        }
        long[] ids = new long[totalSize];
        int index = 0;
        for (List<TemplateUrl> list : displayedEngineLists) {
            for (int i = 0; i < list.size(); i++) {
                // TODO(crbug.com/564878548): Optimise. TemplateUrl#getId() goes through JNI. As
                // this call might end up being a no-op, it would be better if we designed these
                // APIs in a way that avoids throwaway JNI calls in loops.
                ids[index++] = list.get(i).getId();
            }
        }
        SearchEngineSettingsDataProviderJni.get()
                .maybeRecordSettingsPageLoadMetrics(mNativeProvider, ids);
    }

    @NativeMethods
    interface Natives {
        PrepopulatedAndRecentlyVisitedTemplateURLs getPrepopulatedAndRecentlyVisitedTemplateURLs(
                JniPtr<NativeSearchEngineSettingsDataProvider> self);

        @JniType("std::vector<const TemplateURL*>")
        List<TemplateUrl> getTemplateUrlsByCategory(
                JniPtr<NativeSearchEngineSettingsDataProvider> self,
                @JniType("search_engines::TemplateUrlCategory") int category);

        void maybeRecordSettingsPageLoadMetrics(
                JniPtr<NativeSearchEngineSettingsDataProvider> self, long[] displayedEngineIds);
    }
}

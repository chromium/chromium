// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.LifetimeAssert;
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
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeSearchEngineSettingsDataProvider;

    /* package */ SearchEngineSettingsDataProvider(long nativeSearchEngineSettingsDataProvider) {
        assert nativeSearchEngineSettingsDataProvider != 0;
        mNativeSearchEngineSettingsDataProvider = nativeSearchEngineSettingsDataProvider;
    }

    @Override
    public void close() {
        ThreadUtils.assertOnUiThread();
        if (mNativeSearchEngineSettingsDataProvider == 0) return;
        LifetimeAssert.destroy(mLifetimeAssert);
        SearchEngineSettingsDataProviderJni.get().destroy(mNativeSearchEngineSettingsDataProvider);
        mNativeSearchEngineSettingsDataProvider = 0;
    }

    public PrepopulatedAndRecentlyVisitedTemplateURLs
            getPrepopulatedAndRecentlyVisitedTemplateURLs() {
        ThreadUtils.assertOnUiThread();
        assert mNativeSearchEngineSettingsDataProvider != 0;
        return SearchEngineSettingsDataProviderJni.get()
                .getPrepopulatedAndRecentlyVisitedTemplateURLs(
                        mNativeSearchEngineSettingsDataProvider);
    }

    public List<TemplateUrl> getTemplateUrlsByCategory(@TemplateUrlCategory int category) {
        ThreadUtils.assertOnUiThread();
        assert mNativeSearchEngineSettingsDataProvider != 0;
        return SearchEngineSettingsDataProviderJni.get()
                .getTemplateUrlsByCategory(mNativeSearchEngineSettingsDataProvider, category);
    }

    @SafeVarargs
    public final void maybeRecordSettingsPageLoadMetrics(
            List<TemplateUrl>... displayedEngineLists) {
        maybeRecordSettingsPageLoadMetrics(Arrays.asList(displayedEngineLists));
    }

    public void maybeRecordSettingsPageLoadMetrics(List<List<TemplateUrl>> displayedEngineLists) {
        ThreadUtils.assertOnUiThread();
        assert mNativeSearchEngineSettingsDataProvider != 0;
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
                .maybeRecordSettingsPageLoadMetrics(mNativeSearchEngineSettingsDataProvider, ids);
    }

    @NativeMethods
    public interface Natives {
        @NativeClassQualifiedName("search_engines::SearchEngineSettingsDataProvider")
        void destroy(long nativeSearchEngineSettingsDataProvider);

        @NativeClassQualifiedName("search_engines::SearchEngineSettingsDataProvider")
        PrepopulatedAndRecentlyVisitedTemplateURLs getPrepopulatedAndRecentlyVisitedTemplateURLs(
                long nativeSearchEngineSettingsDataProvider);

        @NativeClassQualifiedName("search_engines::SearchEngineSettingsDataProvider")
        @JniType("std::vector<const TemplateURL*>")
        List<TemplateUrl> getTemplateUrlsByCategory(
                long nativeSearchEngineSettingsDataProvider,
                @JniType("search_engines::TemplateUrlCategory") int category);

        @NativeClassQualifiedName("search_engines::SearchEngineSettingsDataProvider")
        void maybeRecordSettingsPageLoadMetrics(
                long nativeSearchEngineSettingsDataProvider, long[] displayedEngineIds);
    }
}

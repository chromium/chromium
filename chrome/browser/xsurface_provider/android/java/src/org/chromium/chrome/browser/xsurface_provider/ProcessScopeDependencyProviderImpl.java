// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.chrome.browser.feed.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;

/** Implementation of {@link ProcessScopeDependencyProvider}. */
// TODO(b/286003870): Stop extending FeedProcessScopeDependencyProvider, and
// remove all dependencies on Feed library.
public class ProcessScopeDependencyProviderImpl extends FeedProcessScopeDependencyProvider {

    private static final String XSURFACE_SPLIT_NAME = "feedv2";

    private final Context mContext;
    private final @Nullable LibraryResolver mLibraryResolver;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final String mApiKey;
    private final ConnectivityManager mConnectivityManager;

    private static boolean sEnableAppFlowDebugging;

    public ProcessScopeDependencyProviderImpl(
            String apiKey, PrivacyPreferencesManager privacyPreferencesManager) {
        mContext = createXsurfaceContext(ContextUtils.getApplicationContext());
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mApiKey = apiKey;
        if (BundleUtils.isIsolatedSplitInstalled(XSURFACE_SPLIT_NAME)) {
            mLibraryResolver =
                    (libName) -> {
                        return BundleUtils.getNativeLibraryPath(libName, XSURFACE_SPLIT_NAME);
                    };
        } else {
            mLibraryResolver = null;
        }
        mConnectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public void logError(String tag, String format, Object... args) {
        Log.e(tag, format, args);
    }

    @Override
    public void logWarning(String tag, String format, Object... args) {
        Log.w(tag, format, args);
    }

    @Override
    public void postTask(int taskType, Runnable task, long delayMs) {
        @TaskTraits int traits;
        switch (taskType) {
            case ProcessScopeDependencyProvider.TASK_TYPE_UI_THREAD:
                traits = TaskTraits.UI_DEFAULT;
                break;
            case ProcessScopeDependencyProvider.TASK_TYPE_BACKGROUND_MAY_BLOCK:
                traits = TaskTraits.BEST_EFFORT_MAY_BLOCK;
                break;
            default:
                assert false : "Invalid task type";
                return;
        }
        PostTask.postDelayedTask(traits, task, delayMs);
    }

    @Override
    public LibraryResolver getLibraryResolver() {
        return mLibraryResolver;
    }

    @Override
    public boolean isXsurfaceUsageAndCrashReportingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.XSURFACE_METRICS_REPORTING)
                && mPrivacyPreferencesManager.isMetricsReportingEnabled();
    }

    public static Context createXsurfaceContext(Context context) {
        return BundleUtils.createContextForInflation(context, XSURFACE_SPLIT_NAME);
    }

    @Override
    public String getGoogleApiKey() {
        return mApiKey;
    }

    @Override
    public String getChromeVersion() {
        return VersionConstants.PRODUCT_VERSION;
    }

    @Override
    public int getChromeChannel() {
        return VersionConstants.CHANNEL;
    }

    @Override
    public int getImageMemoryCacheSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "image_memory_cache_size_percentage",
                /* default= */ 100);
    }

    @Override
    public int getBitmapPoolSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "bitmap_pool_size_percentage",
                /* default= */ 100);
    }

    @Override
    public ProcessScopeDependencyProvider.FeatureStateProvider getFeatureStateProvider() {
        return new ProcessScopeDependencyProvider.FeatureStateProvider() {
            @Override
            public boolean isFeatureActive(String featureName) {
                return ChromeFeatureList.isEnabled(featureName);
            }

            @Override
            public boolean getBooleanParameterValue(
                    String featureName, String paramName, boolean defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        featureName, paramName, defaultValue);
            }

            @Override
            public int getIntegerParameterValue(
                    String featureName, String paramName, int defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        featureName, paramName, defaultValue);
            }

            @Override
            public double getDoubleParameterValue(
                    String featureName, String paramName, double defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        featureName, paramName, defaultValue);
            }
        };
    }

    @Override
    public void reportOnUploadVisibilityLog(@VisibilityLogType int logType, boolean success) {
        // TODO(kamalchoudhury): Rename each of the enum values below to be feed-specific (Example:
        // VisibilityLogType.FEED_VIEW instead of VisibilityLogType.VIEW), given the resulting
        // Histogram is Feed-specific.
        switch (logType) {
            case VisibilityLogType.VIEW:
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.UploadVisibilityLog.View", success);
                break;
            case VisibilityLogType.CLICK:
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.UploadVisibilityLog.Click", success);
                break;
            case VisibilityLogType.UNSPECIFIED:
                // UNSPECIFIED deliberately not reported independently, but will be
                // included in the base UploadVisibilityLog histogram below.
                break;
        }
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.UploadVisibilityLog", success);
    }

    @Override
    public void reportVisibilityLoggingEnabled(boolean enabled) {
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.VisibilityLoggingEnabled", enabled);
    }

    @Override
    public boolean enableAppFlowDebugging() {
        return sEnableAppFlowDebugging;
    }

    @Override
    public boolean isNetworkOnline() {
        NetworkCapabilities capabilities =
                mConnectivityManager.getNetworkCapabilities(
                        mConnectivityManager.getActiveNetwork());
        return capabilities != null
                && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
                && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_SUSPENDED);
    }

    @VisibleForTesting
    public static void setEnableAppFlowDebugging(boolean enable) {
        sEnableAppFlowDebugging = enable;
    }
}

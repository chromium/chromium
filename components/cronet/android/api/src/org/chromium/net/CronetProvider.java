// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.net.http.HttpEngine;
import android.os.Build;
import android.os.ext.SdkExtensions;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.net.impl.CronetLogger;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Provides a factory method to create {@link CronetEngine.Builder} instances. A {@code
 * CronetEngine.Builder} instance can be used to create a specific {@link CronetEngine}
 * implementation. To get the list of available {@link CronetProvider}s call {@link
 * #getAllProviderInfos(Context)}.
 *
 * <p><b>NOTE:</b> This class is for advanced users that want to select a particular Cronet
 * implementation. Most users should simply use {@code new} {@link
 * CronetEngine.Builder#CronetEngine.Builder(android.content.Context)}.
 *
 * <p>{@hide}
 */
public abstract class CronetProvider {
    @VisibleForTesting
    static final String PREFERRED_MINIMUM_HTTPENGINE_VERSION_HTTP_FLAG_NAME =
            "Cronet_PreferredMinimumHttpEngineVersion";

    // Prioritizes providers with higher scores when score-based selection is active.
    @VisibleForTesting static final int RESOURCES_CRONET_PROVIDER_SCORE = 6;
    @VisibleForTesting static final int EMBEDDED_CRONET_PROVIDER_SCORE = 5;
    @VisibleForTesting static final int PREFERRED_HTTP_ENGINE_PROVIDER_SCORE = 4;
    @VisibleForTesting static final int PLAY_SERVICES_CRONET_PROVIDER_SCORE = 3;
    @VisibleForTesting static final int NOT_PREFERRED_HTTP_ENGINE_PROVIDER_SCORE = 2;
    @VisibleForTesting static final int FALLBACK_CRONET_PROVIDER_SCORE = 1;

    /**
     * String returned by {@link CronetProvider#getName} for {@link CronetProvider} that provides
     * native Cronet implementation packaged inside an application. This implementation offers
     * significantly higher performance relative to the fallback Cronet implementations (see {@link
     * #PROVIDER_NAME_FALLBACK}).
     */
    public static final String PROVIDER_NAME_APP_PACKAGED = "App-Packaged-Cronet-Provider";

    /**
     * String returned by {@link CronetProvider#getName} for {@link CronetProvider} that provides
     * Cronet implementation based on the HttpEngine implementation present in the Platform. This
     * implementation doesn't provide functionality which was deemed to be implementation specific,
     * namely access to the netlog and internal metrics. Additionally, support for experimental
     * features is not guaranteed (as with any other Cronet provider).
     */
    public static final String PROVIDER_NAME_HTTPENGINE_NATIVE = "HttpEngine-Native-Provider";

    /**
     * String returned by {@link CronetProvider#getName} for {@link CronetProvider} that provides
     * Cronet implementation based on the system's {@link java.net.HttpURLConnection}
     * implementation. This implementation offers significantly degraded performance relative to
     * native Cronet implementations (see {@link #PROVIDER_NAME_APP_PACKAGED}).
     */
    public static final String PROVIDER_NAME_FALLBACK = "Fallback-Cronet-Provider";

    /**
     * The name of an optional key in the app string resource file that contains the class name of
     * an alternative {@code CronetProvider} implementation.
     */
    private static final String RES_KEY_CRONET_IMPL_CLASS = "CronetProviderClassName";

    private static final String TAG = CronetProvider.class.getSimpleName();

    protected final Context mContext;

    protected CronetProvider(Context context) {
        if (context == null) {
            throw new IllegalArgumentException("Context must not be null");
        }
        mContext = context;
    }

    /**
     * Creates and returns an instance of {@link CronetEngine.Builder}.
     * <p/>
     * <b>NOTE:</b> This class is for advanced users that want to select a particular
     * Cronet implementation. Most users should simply use {@code new} {@link
     * CronetEngine.Builder#CronetEngine.Builder(android.content.Context)}.
     *
     * @return {@code CronetEngine.Builder}.
     * @throws IllegalStateException if the provider is not enabled (see {@link #isEnabled}.
     */
    public abstract CronetEngine.Builder createBuilder();

    /**
     * Returns the provider name. The well-know provider names include:
     * <ul>
     *     <li>{@link #PROVIDER_NAME_APP_PACKAGED}</li>
     *     <li>{@link #PROVIDER_NAME_FALLBACK}</li>
     * </ul>
     *
     * @return provider name.
     */
    public abstract String getName();

    /**
     * Returns the provider version. The version can be used to select the newest available provider
     * if multiple providers are available.
     *
     * @return provider version.
     */
    public abstract String getVersion();

    /**
     * Returns whether the provider is enabled and can be used to instantiate the Cronet engine. A
     * provider being out-of-date (older than the API) and needing updating is one potential reason
     * it could be disabled. Please read the provider documentation for enablement procedure.
     *
     * @return {@code true} if the provider is enabled.
     */
    public abstract boolean isEnabled();

    @Override
    public String toString() {
        return "["
                + "class="
                + getClass().getName()
                + ", "
                + "name="
                + getName()
                + ", "
                + "version="
                + getVersion()
                + ", "
                + "enabled="
                + isEnabled()
                + "]";
    }

    /** Name of the Java {@link CronetProvider} class. */
    private static final String JAVA_CRONET_PROVIDER_CLASS =
            "org.chromium.net.impl.JavaCronetProvider";

    /** Name of the HttpEngine {@link CronetProvider} class. */
    private static final String HTTPENGINE_PROVIDER_CLASS =
            "org.chromium.net.impl.HttpEngineNativeProvider";

    /** Name of the native {@link CronetProvider} class. */
    private static final String NATIVE_CRONET_PROVIDER_CLASS =
            "org.chromium.net.impl.NativeCronetProvider";

    /** {@link CronetProvider} class that is packaged with Google Play Services. */
    private static final String PLAY_SERVICES_CRONET_PROVIDER_CLASS =
            "com.google.android.gms.net.PlayServicesCronetProvider";

    /**
     * {@link CronetProvider} a deprecated class that may be packaged with some old versions of
     * Google Play Services.
     */
    private static final String GMS_CORE_CRONET_PROVIDER_CLASS =
            "com.google.android.gms.net.GmsCoreCronetProvider";

    static final class ProviderInfo {
        public CronetProvider provider;
        // Populates the provider score to determine sort order.
        // When score-based selection is active, providers are sorted by score in descending order.
        // Note: we compute scores in this code. We do not let providers decide their own score,
        // because providers are not shipped alongside this code - changing them would require
        // tedious release coordination.
        public int providerScore;
        public CronetLogger.CronetSource logSource;

        // Delegate ProviderInfo comparisons to `provider`. This actually matters in some cases such
        // as PLAY_SERVICES vs GMS_CORE, see b/329440572.

        @Override
        public int hashCode() {
            return provider.hashCode();
        }

        @Override
        public boolean equals(@Nullable Object other) {
            return other instanceof ProviderInfo
                    && provider.equals(((ProviderInfo) other).provider);
        }
    }

    /**
     * Returns an unmodifiable list of all available {@link CronetProvider}s. The providers are
     * returned in no particular order. Some of the returned providers may be in a disabled state
     * and should be enabled by the invoker. See {@link CronetProvider#isEnabled()}.
     *
     * @return the list of available providers.
     */
    public static List<CronetProvider> getAllProviders(Context context) {
        var providers = new ArrayList<CronetProvider>();
        for (var providerInfo : getAllProviderInfos(context)) {
            providers.add(providerInfo.provider);
        }
        return Collections.unmodifiableList(providers);
    }

    /**
     * Same as {@link #getAllProviders}, but returning the providerInfos directly.
     *
     * @return the list of available providerInfos.
     */
    static List<ProviderInfo> getAllProviderInfos(Context context) {
        // Use LinkedHashSet to preserve the order and eliminate duplicate providers.
        Set<ProviderInfo> providers = new LinkedHashSet<>();
        addCronetProviderFromResourceFile(
                context, CronetLogger.CronetSource.CRONET_SOURCE_UNSPECIFIED, providers);
        addCronetProviderImplByClassName(
                context,
                PLAY_SERVICES_CRONET_PROVIDER_CLASS,
                /* score= */ PLAY_SERVICES_CRONET_PROVIDER_SCORE,
                CronetLogger.CronetSource.CRONET_SOURCE_PLAY_SERVICES,
                providers,
                false);
        addCronetProviderImplByClassName(
                context,
                GMS_CORE_CRONET_PROVIDER_CLASS,
                /* score= */ PLAY_SERVICES_CRONET_PROVIDER_SCORE,
                CronetLogger.CronetSource.CRONET_SOURCE_PLAY_SERVICES,
                providers,
                false);
        addCronetProviderImplByClassName(
                context,
                NATIVE_CRONET_PROVIDER_CLASS,
                /* score= */ EMBEDDED_CRONET_PROVIDER_SCORE,
                CronetLogger.CronetSource.CRONET_SOURCE_STATICALLY_LINKED,
                providers,
                false);
        addCronetProviderImplByClassName(
                context,
                HTTPENGINE_PROVIDER_CLASS,
                /* score= */ calculateHttpEngineNativeProviderScore(context),
                CronetLogger.CronetSource.CRONET_SOURCE_PLATFORM,
                providers,
                false);
        addCronetProviderImplByClassName(
                context,
                JAVA_CRONET_PROVIDER_CLASS,
                /* score= */ FALLBACK_CRONET_PROVIDER_SCORE,
                CronetLogger.CronetSource.CRONET_SOURCE_FALLBACK,
                providers,
                false);
        return Collections.unmodifiableList(new ArrayList<>(providers));
    }

    private static int calculateHttpEngineNativeProviderScore(Context context) {
        return calculateHttpEngineNativeProviderScoreInternal(context, /*versionOverrideForTesting=*/null);
    }

    @VisibleForTesting
    static int calculateHttpEngineNativeProviderScoreInternal(Context context, String versionOverrideForTesting) {
        // This logic must be kept in sync with HttpEngineNativeProvider#isEnabled(). Direct
        // invocation is avoided to prevent circular dependencies between CronetAPI and the
        // HttpEngineNativeProvider  implementation, simplifying library integration. Resolving such
        // a dependency would typically involve merging the libraries with their downstream
        // dependencies, which is a complex and tedious process.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R
                || SdkExtensions.getExtensionVersion(Build.VERSION_CODES.S) < 7) {
            // HttpEngine is not available on the device, go for the lowest score available.
            // HttpEngineNativeProvider#isEnabled() will return false anyway
            return Integer.MIN_VALUE;
        }

        var minimumHttpEngineVersionFlag =
                HttpFlagsForApi.getHttpFlags(context)
                        .flags()
                        .get(PREFERRED_MINIMUM_HTTPENGINE_VERSION_HTTP_FLAG_NAME);
        var minimumHttpEngineVersion =
                minimumHttpEngineVersionFlag == null
                        // This is the version of HttpEngine that shipped in B+ devices
                        // with preloading enabled. We'll be using this as the default
                        // minimum preferred HttpEngine version.
                        ? "133.0.6876.3"
                        : minimumHttpEngineVersionFlag.getStringValue();
        return compareVersions(
                        (versionOverrideForTesting != null ? versionOverrideForTesting : HttpEngine.getVersionString()),
                        minimumHttpEngineVersion)
                >= 0 ? PREFERRED_HTTP_ENGINE_PROVIDER_SCORE : NOT_PREFERRED_HTTP_ENGINE_PROVIDER_SCORE;
    }

    /**
     * Compares two strings that contain versions. The string should only contain dot-separated
     * segments that contain an arbitrary number of digits digits [0-9].
     *
     * @param s1 the first string.
     * @param s2 the second string.
     * @return -1 if s1<s2, +1 if s1>s2 and 0 if s1=s2. If two versions are equal, the version with
     *     the higher number of segments is considered to be higher.
     * @throws IllegalArgumentException if any of the strings contains an illegal version number.
     */
    @VisibleForTesting
    static int compareVersions(String s1, String s2) {
        if (s1 == null || s2 == null) {
            throw new IllegalArgumentException("The input values cannot be null");
        }
        String[] s1segments = s1.split("\\.");
        String[] s2segments = s2.split("\\.");
        if (s1segments.length != s2segments.length) {
            throw new IllegalArgumentException(
                    "Version strings must have an equal number of segments for comparison: "
                            + s1
                            + " vs "
                            + s2);
        }
        for (int i = 0; i < s1segments.length; i++) {
            try {
                int s1segment = Integer.parseInt(s1segments[i]);
                int s2segment = Integer.parseInt(s2segments[i]);
                if (s1segment != s2segment) {
                    return Integer.compare(s1segment, s2segment);
                }
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException(
                        "Unable to convert version segments into"
                                + " integers: "
                                + s1segments[i]
                                + " & "
                                + s2segments[i],
                        e);
            }
        }
        // If we reached this point then all segments are equal which means equal versions.
        return 0;
    }

    /**
     * Attempts to add a new provider referenced by the class name to a set.
     *
     * @param className the class name of the provider that should be instantiated.
     * @param providers the set of providers to add the new provider to.
     * @return {@code true} if the provider was added to the set; {@code false} if the provider
     *     couldn't be instantiated.
     */
    private static boolean addCronetProviderImplByClassName(
            Context context,
            String className,
            int score,
            CronetLogger.CronetSource logSource,
            Set<ProviderInfo> providers,
            boolean logError) {
        ClassLoader loader = context.getClassLoader();
        try {
            Class<? extends CronetProvider> providerClass =
                    loader.loadClass(className).asSubclass(CronetProvider.class);
            Constructor<? extends CronetProvider> ctor =
                    providerClass.getConstructor(Context.class);
            var providerInfo = new ProviderInfo();
            providerInfo.provider = ctor.newInstance(context);
            providerInfo.providerScore = score;
            providerInfo.logSource = logSource;
            providers.add(providerInfo);
            return true;
        } catch (InstantiationException e) {
            logReflectiveOperationException(className, logError, e);
        } catch (InvocationTargetException e) {
            logReflectiveOperationException(className, logError, e);
        } catch (NoSuchMethodException e) {
            logReflectiveOperationException(className, logError, e);
        } catch (IllegalAccessException e) {
            logReflectiveOperationException(className, logError, e);
        } catch (ClassNotFoundException e) {
            logReflectiveOperationException(className, logError, e);
        }
        return false;
    }

    /**
     * De-duplicates exception handling logic in {@link #addCronetProviderImplByClassName}. It
     * should be removed when support of API Levels lower than 19 is deprecated.
     */
    private static void logReflectiveOperationException(
            String className, boolean logError, Exception e) {
        if (logError) {
            Log.e(TAG, "Unable to load provider class: " + className, e);
        } else {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(
                        TAG,
                        "Tried to load "
                                + className
                                + " provider class but it wasn't"
                                + " included in the app classpath");
            }
        }
    }

    /**
     * Attempts to add a provider specified in the app resource file to a set.
     *
     * @param providers the set of providers to add the new provider to.
     * @return {@code true} if the provider was added to the set; {@code false} if the app resources
     *     do not include the string with {@link #RES_KEY_CRONET_IMPL_CLASS} key.
     * @throws RuntimeException if the provider cannot be found or instantiated.
     */
    // looking up resources from other apps requires the use of getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    private static boolean addCronetProviderFromResourceFile(
            Context context, CronetLogger.CronetSource logSource, Set<ProviderInfo> providers) {
        int resId =
                context.getResources()
                        .getIdentifier(
                                RES_KEY_CRONET_IMPL_CLASS, "string", context.getPackageName());
        // Resource not found
        if (resId == 0) {
            // The resource wasn't included in the app; therefore, there is nothing to add.
            return false;
        }
        String className = context.getString(resId);

        // If the resource specifies a well known provider, don't load it because
        // there will be an attempt to load it anyways.
        if (className == null
                || className.equals(PLAY_SERVICES_CRONET_PROVIDER_CLASS)
                || className.equals(GMS_CORE_CRONET_PROVIDER_CLASS)
                || className.equals(JAVA_CRONET_PROVIDER_CLASS)
                || className.equals(NATIVE_CRONET_PROVIDER_CLASS)
                || className.equals(HTTPENGINE_PROVIDER_CLASS)) {
            return false;
        }

        if (!addCronetProviderImplByClassName(
                context,
                className,
                /* score= */ RESOURCES_CRONET_PROVIDER_SCORE,
                logSource,
                providers,
                true)) {
            Log.e(
                    TAG,
                    "Unable to instantiate Cronet implementation class "
                            + className
                            + " that is listed as in the app string resource file under "
                            + RES_KEY_CRONET_IMPL_CLASS
                            + " key");
        }
        return true;
    }
}

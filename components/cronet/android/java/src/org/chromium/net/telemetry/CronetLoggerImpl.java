// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import android.os.Build;
import android.util.Log;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.ConnectionCloseSource;
import org.chromium.net.impl.CronetLogger;

import java.util.List;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicInteger;

/** Logger for logging cronet's telemetry */
@RequiresApi(Build.VERSION_CODES.R)
public class CronetLoggerImpl extends CronetLogger {
    private static final String TAG = CronetLoggerImpl.class.getSimpleName();

    private final AtomicInteger mSamplesRateLimited = new AtomicInteger();
    private final RateLimiter mRateLimiter;

    public CronetLoggerImpl(int sampleRatePerSecond) {
        this(new RateLimiter(sampleRatePerSecond));
    }

    @VisibleForTesting
    public CronetLoggerImpl(RateLimiter rateLimiter) {
        super();
        this.mRateLimiter = rateLimiter;
    }

    @Override
    public long generateId() {
        // Pick an ID at random, but avoid Long.MIN_VALUE, Long.MAX_VALUE, 0 and -1, as these may
        // be confused with values people may think of as sentinels.
        long id = ThreadLocalRandom.current().nextLong(Long.MIN_VALUE + 1, Long.MAX_VALUE - 2);
        return id >= -1 ? id + 2 : id;
    }

    @Override
    public void logCronetEngineBuilderInitializedInfo(CronetEngineBuilderInitializedInfo info) {
        CronetStatsLog.write(
                CronetStatsLog.CRONET_ENGINE_BUILDER_INITIALIZED,
                info.cronetInitializationRef,
                convertToProtoCronetEngineBuilderInitializedAuthor(info.author),
                info.engineBuilderCreatedLatencyMillis,
                convertToProtoCronetEngineBuilderInitializedSource(info.source),
                OptionalBoolean.fromBoolean(info.creationSuccessful).getValue(),
                info.apiVersion.getMajorVersion(),
                info.apiVersion.getMinorVersion(),
                info.apiVersion.getBuildVersion(),
                info.apiVersion.getPatchVersion(),
                // These null checks actually matter. See b/329601514.
                info.implVersion == null ? -1 : info.implVersion.getMajorVersion(),
                info.implVersion == null ? -1 : info.implVersion.getMinorVersion(),
                info.implVersion == null ? -1 : info.implVersion.getBuildVersion(),
                info.implVersion == null ? -1 : info.implVersion.getPatchVersion(),
                info.uid);
    }

    @Override
    public void logCronetInitializedInfo(CronetInitializedInfo info) {
        // This atom uses arrays, which are only supported by StatsLog starting from Android T. If
        // we are running Android <T we simply drop the atom, which is fine-ish because it doesn't
        // carry critical information, nor does it carry information that other atoms may want to
        // join against.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return;

        CronetStatsLog.write(
                CronetStatsLog.CRONET_INITIALIZED,
                info.cronetInitializationRef,
                info.engineCreationLatencyMillis,
                info.engineAsyncLatencyMillis,
                info.httpFlagsLatencyMillis,
                OptionalBoolean.fromBoolean(info.httpFlagsSuccessful).getValue(),
                longListToLongArray(info.httpFlagsNames),
                longListToLongArray(info.httpFlagsValues));
    }

    @Override
    public void logCronetEngineCreation(
            long cronetEngineId,
            CronetEngineBuilderInfo builder,
            CronetVersion version,
            CronetSource source) {
        if (builder == null || version == null || source == null) {
            return;
        }

        writeCronetEngineCreation(cronetEngineId, builder, version, source);
    }

    @Override
    public void logCronetTrafficInfo(long cronetEngineId, CronetTrafficInfo trafficInfo) {
        if (trafficInfo == null) {
            return;
        }

        if (!mRateLimiter.tryAcquire()) {
            mSamplesRateLimited.incrementAndGet();
            return;
        }

        writeCronetTrafficReported(cronetEngineId, trafficInfo, mSamplesRateLimited.getAndSet(0));
    }

    @SuppressWarnings("CatchingUnchecked")
    public void writeCronetEngineCreation(
            long cronetEngineId,
            CronetEngineBuilderInfo builder,
            CronetVersion version,
            CronetSource source) {
        try {
            // Parse experimental Options
            ExperimentalOptions experimentalOptions =
                    new ExperimentalOptions(builder.getExperimentalOptions());

            CronetStatsLog.write(
                    CronetStatsLog.CRONET_ENGINE_CREATED,
                    cronetEngineId,
                    version.getMajorVersion(),
                    version.getMinorVersion(),
                    version.getBuildVersion(),
                    version.getPatchVersion(),
                    convertToProtoCronetEngineCreatedSource(source),
                    builder.isBrotliEnabled(),
                    builder.isHttp2Enabled(),
                    convertToProtoHttpCacheMode(builder.getHttpCacheMode()),
                    builder.isPublicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                    builder.isQuicEnabled(),
                    builder.isNetworkQualityEstimatorEnabled(),
                    builder.getThreadPriority(),
                    // QUIC options
                    experimentalOptions.getConnectionOptionsOption(),
                    experimentalOptions.getStoreServerConfigsInPropertiesOption().getValue(),
                    experimentalOptions.getMaxServerConfigsStoredInPropertiesOption(),
                    experimentalOptions.getIdleConnectionTimeoutSecondsOption(),
                    experimentalOptions.getGoawaySessionsOnIpChangeOption().getValue(),
                    experimentalOptions.getCloseSessionsOnIpChangeOption().getValue(),
                    experimentalOptions.getMigrateSessionsOnNetworkChangeV2Option().getValue(),
                    experimentalOptions.getMigrateSessionsEarlyV2().getValue(),
                    experimentalOptions.getDisableBidirectionalStreamsOption().getValue(),
                    experimentalOptions.getMaxTimeBeforeCryptoHandshakeSecondsOption(),
                    experimentalOptions.getMaxIdleTimeBeforeCryptoHandshakeSecondsOption(),
                    experimentalOptions.getEnableSocketRecvOptimizationOption().getValue(),
                    // AsyncDNS
                    experimentalOptions.getAsyncDnsEnableOption().getValue(),
                    // StaleDNS
                    experimentalOptions.getStaleDnsEnableOption().getValue(),
                    experimentalOptions.getStaleDnsDelayMillisOption(),
                    experimentalOptions.getStaleDnsMaxExpiredTimeMillisOption(),
                    experimentalOptions.getStaleDnsMaxStaleUsesOption(),
                    experimentalOptions.getStaleDnsAllowOtherNetworkOption().getValue(),
                    experimentalOptions.getStaleDnsPersistToDiskOption().getValue(),
                    experimentalOptions.getStaleDnsPersistDelayMillisOption(),
                    experimentalOptions.getStaleDnsUseStaleOnNameNotResolvedOption().getValue(),
                    experimentalOptions.getDisableIpv6OnWifiOption().getValue(),
                    builder.getCronetInitializationRef());
        } catch (Exception e) { // catching all exceptions since we don't want to crash the client
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(
                        TAG,
                        String.format(
                                "Failed to log CronetEngine:%s creation: %s",
                                cronetEngineId, e.getMessage()));
            }
        }
    }

    @SuppressWarnings("CatchingUnchecked")
    @VisibleForTesting
    public void writeCronetTrafficReported(
            long cronetEngineId, CronetTrafficInfo trafficInfo, int samplesRateLimitedCount) {
        try {
            CronetStatsLog.write(
                    CronetStatsLog.CRONET_TRAFFIC_REPORTED,
                    cronetEngineId,
                    SizeBuckets.calcRequestHeadersSizeBucket(
                            trafficInfo.getRequestHeaderSizeInBytes()),
                    SizeBuckets.calcRequestBodySizeBucket(trafficInfo.getRequestBodySizeInBytes()),
                    SizeBuckets.calcResponseHeadersSizeBucket(
                            trafficInfo.getResponseHeaderSizeInBytes()),
                    SizeBuckets.calcResponseBodySizeBucket(
                            trafficInfo.getResponseBodySizeInBytes()),
                    trafficInfo.getResponseStatusCode(),
                    Hash.hash(trafficInfo.getNegotiatedProtocol()),
                    (int) trafficInfo.getHeadersLatency().toMillis(),
                    (int) trafficInfo.getTotalLatency().toMillis(),
                    trafficInfo.wasConnectionMigrationAttempted(),
                    trafficInfo.didConnectionMigrationSucceed(),
                    samplesRateLimitedCount,
                    convertToProtoCronetRequestTerminalState(trafficInfo.getTerminalState()),
                    trafficInfo.getNonfinalUserCallbackExceptionCount(),
                    /* total_idle_time_millis= */ -1,
                    /* total_user_executor_execute_latency_millis= */ -1,
                    trafficInfo.getReadCount(),
                    trafficInfo.getOnUploadReadCount(),
                    OptionalBoolean.fromBoolean(trafficInfo.getIsBidiStream()).getValue(),
                    OptionalBoolean.fromBoolean(trafficInfo.getFinalUserCallbackThrew()).getValue(),
                    trafficInfo.getUid(),
                    trafficInfo.getNetworkInternalErrorCode(),
                    trafficInfo.getQuicErrorCode(),
                    convertToProtoConnectionCloseSource(trafficInfo.getConnectionCloseSource()),
                    convertToProtoFailureReason(trafficInfo.getFailureReason()),
                    OptionalBoolean.fromBoolean(trafficInfo.getIsSocketReused()).getValue());
        } catch (Exception e) {
            // using addAndGet because another thread might have modified samplesRateLimited's value
            mSamplesRateLimited.addAndGet(samplesRateLimitedCount);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(
                        TAG,
                        String.format(
                                "Failed to log cronet traffic sample for CronetEngine %s: %s",
                                cronetEngineId, e.getMessage()));
            }
        }
    }

    private static int convertToProtoFailureReason(
            CronetTrafficInfo.RequestFailureReason failureReason) {
        switch (failureReason) {
            case NETWORK:
                return CronetStatsLog
                        .CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_NETWORK;
            case OTHER:
                return CronetStatsLog.CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_OTHER;
            default:
                return CronetStatsLog
                        .CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_UNKNOWN;
        }
    }

    private static int convertToProtoConnectionCloseSource(@ConnectionCloseSource int source) {
        switch (source) {
            case ConnectionCloseSource.SELF:
                return CronetStatsLog
                        .CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_SELF;
            case ConnectionCloseSource.PEER:
                return CronetStatsLog
                        .CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_PEER;
        }
        return CronetStatsLog
                .CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_UNKNOWN;
    }

    private static int convertToProtoCronetEngineBuilderInitializedAuthor(
            CronetEngineBuilderInitializedInfo.Author author) {
        switch (author) {
            case API:
                return CronetStatsLog.CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_API;
            case IMPL:
                return CronetStatsLog.CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_IMPL;
        }
        return CronetStatsLog.CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_UNSPECIFIED;
    }

    private static int convertToProtoCronetRequestTerminalState(
            CronetTrafficInfo.RequestTerminalState requestTerminalState) {
        switch (requestTerminalState) {
            case SUCCEEDED:
                return CronetStatsLog.CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_SUCCEEDED;
            case ERROR:
                return CronetStatsLog.CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_ERROR;
            case CANCELLED:
                return CronetStatsLog.CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_CANCELLED;
            default:
                return CronetStatsLog.CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_UNKNOWN;
        }
    }

    private static int convertToProtoCronetEngineBuilderInitializedSource(CronetSource source) {
        switch (source) {
            case CRONET_SOURCE_STATICALLY_LINKED:
                return CronetStatsLog
                        .CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_EMBEDDED_NATIVE;
            case CRONET_SOURCE_PLAY_SERVICES:
                return CronetStatsLog
                        .CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_GMSCORE_NATIVE;
            case CRONET_SOURCE_FALLBACK:
                return CronetStatsLog
                        .CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_EMBEDDED_JAVA;
            case CRONET_SOURCE_PLATFORM:
                return CronetStatsLog
                        .CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_HTTPENGINE_NATIVE;
            default:
                return CronetStatsLog
                        .CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_UNSPECIFIED;
        }
    }

    private static int convertToProtoCronetEngineCreatedSource(CronetSource source) {
        switch (source) {
            case CRONET_SOURCE_STATICALLY_LINKED:
                return CronetStatsLog
                        .CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_STATICALLY_LINKED;
            case CRONET_SOURCE_PLAY_SERVICES:
                return CronetStatsLog.CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_GMSCORE_DYNAMITE;
            case CRONET_SOURCE_FALLBACK:
                return CronetStatsLog.CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_FALLBACK;
            case CRONET_SOURCE_UNSPECIFIED:
                return CronetStatsLog.CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_UNSPECIFIED;
            default:
                return CronetStatsLog.CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_UNSPECIFIED;
        }
    }

    private static int convertToProtoHttpCacheMode(int httpCacheMode) {
        switch (httpCacheMode) {
            case 0:
                return CronetStatsLog.CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISABLED;
            case 1:
                return CronetStatsLog.CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISK;
            case 2:
                return CronetStatsLog
                        .CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISK_NO_HTTP;
            case 3:
                return CronetStatsLog.CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_IN_MEMORY;
            default:
                throw new IllegalArgumentException("Expected httpCacheMode to range from 0 to 3");
        }
    }

    // Shamelessly copy-pasted from //base/android/java/src/org/chromium/base/CollectionUtil.java
    // to avoid adding a large dependency on //base.
    private static long[] longListToLongArray(List<Long> list) {
        long[] array = new long[list.size()];
        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }
        return array;
    }
}

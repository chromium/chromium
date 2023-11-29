// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.os.Build;
import android.util.Log;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.impl.CronetLogger;

import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.atomic.AtomicInteger;

/** Logger for logging cronet's telemetry */
@RequiresApi(Build.VERSION_CODES.R)
public class CronetLoggerImpl extends CronetLogger {
    private static final String TAG = CronetLoggerImpl.class.getSimpleName();

    private static final MessageDigest MD5_MESSAGE_DIGEST;

    static {
        MessageDigest messageDigest;
        try {
            messageDigest = MessageDigest.getInstance("MD5");
        } catch (NoSuchAlgorithmException e) {
            Log.d(TAG, "Error while instantiating messageDigest", e);
            messageDigest = null;
        }
        MD5_MESSAGE_DIGEST = messageDigest;
    }

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
    public void logCronetEngineCreation(
            int cronetEngineId,
            CronetEngineBuilderInfo builder,
            CronetVersion version,
            CronetSource source) {
        if (builder == null || version == null || source == null) {
            return;
        }

        writeCronetEngineCreation(cronetEngineId, builder, version, source);
    }

    @Override
    public void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo) {
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
                    convertToProtoCronetSource(source),
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
                    /* cronet_initialization_ref= */ -1);
        } catch (Exception e) { // catching all exceptions since we don't want to crash the client
            Log.d(
                    TAG,
                    String.format(
                            "Failed to log CronetEngine:%s creation: %s",
                            cronetEngineId, e.getMessage()));
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
                    hashNegotiatedProtocol(trafficInfo.getNegotiatedProtocol()),
                    (int) trafficInfo.getHeadersLatency().toMillis(),
                    (int) trafficInfo.getTotalLatency().toMillis(),
                    trafficInfo.wasConnectionMigrationAttempted(),
                    trafficInfo.didConnectionMigrationSucceed(),
                    samplesRateLimitedCount,
                    /* terminal_state= */ CronetStatsLog
                            .CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_UNKNOWN,
                    /* user_callback_exception_count= */ -1,
                    /* total_idle_time_millis= */ -1,
                    /* total_user_executor_execute_latency_millis= */ -1,
                    /* read_count= */ -1,
                    /* on_upload_read_count= */ -1,
                    /* is_bidi_stream= */ CronetStatsLog
                            .CRONET_TRAFFIC_REPORTED__IS_BIDI_STREAM__OPTIONAL_BOOLEAN_UNSET);
        } catch (Exception e) {
            // using addAndGet because another thread might have modified samplesRateLimited's value
            mSamplesRateLimited.addAndGet(samplesRateLimitedCount);
            Log.d(
                    TAG,
                    String.format(
                            "Failed to log cronet traffic sample for CronetEngine %s: %s",
                            cronetEngineId, e.getMessage()));
        }
    }

    private static int convertToProtoCronetSource(CronetSource source) {
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

    private static long hashNegotiatedProtocol(String protocol) {
        if (MD5_MESSAGE_DIGEST == null || protocol == null || protocol.isEmpty()) {
            return 0L;
        }

        byte[] md = MD5_MESSAGE_DIGEST.digest(protocol.getBytes(UTF_8));
        return ByteBuffer.wrap(md).getLong();
    }
}

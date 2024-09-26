// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.content.SharedPreferences;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.components.variations.VariationsCompressionUtils;
import org.chromium.components.variations.VariationsCompressionUtils.DeltaPatchException;
import org.chromium.components.variations.VariationsCompressionUtils.InstanceManipulations;
import org.chromium.components.variations.VariationsCompressionUtils.InvalidImHeaderException;
import org.chromium.components.variations.VariationsSeedOuterClass.VariationsSeed;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.IOException;
import java.io.InputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Objects;

/** Fetches the variations seed before the actual first run of Chrome. */
public class VariationsSeedFetcher {
    private static final String TAG = "VariationsSeedFetch";

    // Note: ChromeVariations = 2 means "Disable all variations".
    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "chrome_variations_android",
                    """
                    semantics {
                      sender: "Chrome Variations Service (Android)"
                      description:
                          "The variations service is responsible for determining the state of "
                          "field trials in Chrome. These field trials typically configure either "
                          "A/B experiments, or launched features – oftentimes, critical security "
                          "features."
                      trigger:
                        "This request is made once, on Chrome's first run, to determine the "
                        "initial state Chrome should be in."
                      data: "None."
                      destination: GOOGLE_OWNED_SERVICE
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "Cannot be disabled in Settings. Chrome Variations are an essential part "
                        "of Chrome releases."
                      chrome_policy {
                        ChromeVariations {
                          ChromeVariations: 2
                        }
                      }
                    }""");

    @IntDef({VariationsPlatform.ANDROID, VariationsPlatform.ANDROID_WEBVIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VariationsPlatform {
        int ANDROID = 0;
        int ANDROID_WEBVIEW = 1;
    }

    private static final String DEFAULT_VARIATIONS_SERVER_URL =
            "https://clientservices.googleapis.com/chrome-variations/seed";

    private static final String DEFAULT_FAST_VARIATIONS_SERVER_URL =
            "https://clientservices.googleapis.com/chrome-variations/fastfinch/seed";

    private static final int READ_TIMEOUT = 3000; // time in ms
    private static final int REQUEST_TIMEOUT = 1000; // time in ms

    @VisibleForTesting
    public static final String SEED_FETCH_RESULT_HISTOGRAM = "Variations.FirstRun.SeedFetchResult";

    // Values for the "Variations.FirstRun.SeedFetchResult" sparse histogram, which also logs
    // HTTP result codes. These are negative so that they don't conflict with the HTTP codes.
    // These values should not be renumbered or re-used since they are logged to UMA.
    @VisibleForTesting public static final int SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION = -6;
    @VisibleForTesting public static final int SEED_FETCH_RESULT_INVALID_IM_HEADER = -5;
    // private static final int SEED_FETCH_RESULT_INVALID_DATE_HEADER = -4;
    private static final int SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION = -3;
    private static final int SEED_FETCH_RESULT_TIMEOUT = -2;
    @VisibleForTesting public static final int SEED_FETCH_RESULT_IOEXCEPTION = -1;

    @VisibleForTesting static final String VARIATIONS_INITIALIZED_PREF = "variations_initialized";

    @VisibleForTesting
    public static final String SEED_FETCH_DELTA_COMPRESSION =
            "Variations.FirstRun.DeltaCompression";

    // UMA constant for logging the request and response status of delta compression when requesting
    // a finch seed.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @VisibleForTesting
    @IntDef({
        DeltaCompression.REQUESTED_RECEIVED,
        DeltaCompression.REQUESTED_NOT_RECEIVED,
        DeltaCompression.NOT_REQUESTED_RECEIVED,
        DeltaCompression.NOT_REQUESTED_NOT_RECEIVED,
        DeltaCompression.NUM_ENTRIES
    })
    public @interface DeltaCompression {
        int REQUESTED_RECEIVED = 0;
        int REQUESTED_NOT_RECEIVED = 1;
        int NOT_REQUESTED_RECEIVED = 2;
        int NOT_REQUESTED_NOT_RECEIVED = 3;
        int NUM_ENTRIES = 4;
    }

    /** For mocking the Date in tests. */
    @VisibleForTesting
    public interface DateTime {
        Date newDate();
    }

    private DateTime mDateTime = () -> new Date();

    /**
     * Overwrite the DateTime, typically with a mock for testing.
     *
     * @param dateTime the mock.
     */
    @VisibleForTesting
    public void setDateTime(DateTime dateTime) {
        // Used for testing, to inject mock Date()
        mDateTime = dateTime;
    }

    /** Get the dateTime, for testing only. */
    @VisibleForTesting
    public DateTime getDateTime() {
        return mDateTime;
    }

    // Synchronization lock to make singleton thread-safe.
    private static final Object sLock = new Object();

    private static VariationsSeedFetcher sInstance;

    @VisibleForTesting
    public VariationsSeedFetcher() {}

    public static VariationsSeedFetcher get() {
        // TODO(aberent) Check not running on UI thread. Doing so however makes Robolectric testing
        // of dependent classes difficult.
        synchronized (sLock) {
            if (sInstance == null) {
                sInstance = new VariationsSeedFetcher();
            }
            return sInstance;
        }
    }

    /**
     * Override the VariationsSeedFetcher, typically with a mock, for testing classes that depend on
     * this one.
     *
     * @param fetcher the mock.
     */
    public static void setVariationsSeedFetcherForTesting(VariationsSeedFetcher fetcher) {
        var oldValue = sInstance;
        sInstance = fetcher;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @VisibleForTesting
    protected HttpURLConnection getServerConnection(SeedFetchParameters params)
            throws MalformedURLException, IOException {
        String urlString = getConnectionString(params);
        URL url = new URL(urlString);
        return (HttpURLConnection) ChromiumNetworkAdapter.openConnection(url, TRAFFIC_ANNOTATION);
    }

    @VisibleForTesting
    protected List<String> getAvailableInstanceManipulations() {
        List<String> compressions = new ArrayList<String>();
        if (CommandLine.getInstance()
                .hasSwitch(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)) {
            compressions.add(VariationsCompressionUtils.DELTA_COMPRESSION_HEADER);
        }
        compressions.add(VariationsCompressionUtils.GZIP_COMPRESSION_HEADER);
        return compressions;
    }

    @VisibleForTesting
    protected String getConnectionString(SeedFetchParameters params) {
        // TODO(crbug.com/40825562): Consider reusing native
        // VariationsService::GetVariationsServerURL().
        String urlString;
        if (CommandLine.getInstance().hasSwitch(VariationsSwitches.VARIATIONS_SERVER_URL)) {
            urlString =
                    CommandLine.getInstance()
                            .getSwitchValue(VariationsSwitches.VARIATIONS_SERVER_URL);
        } else if (params.mIsFastFetchMode) {
            urlString = DEFAULT_FAST_VARIATIONS_SERVER_URL;
        } else {
            urlString = DEFAULT_VARIATIONS_SERVER_URL;
        }

        urlString += "?osname=";
        switch (params.mPlatform) {
            case VariationsPlatform.ANDROID:
                urlString += "android";
                break;
            case VariationsPlatform.ANDROID_WEBVIEW:
                urlString += "android_webview";
                break;
            default:
                assert false;
        }
        if (params.mRestrictMode != null && !params.mRestrictMode.isEmpty()) {
            urlString += "&restrict=" + params.mRestrictMode;
        }
        if (params.mMilestone != null && !params.mMilestone.isEmpty()) {
            urlString += "&milestone=" + params.mMilestone;
        }

        String forcedChannel =
                CommandLine.getInstance()
                        .getSwitchValue(VariationsSwitches.FAKE_VARIATIONS_CHANNEL);
        if (forcedChannel != null) {
            params.mChannel = forcedChannel;
        }
        if (params.mChannel != null && !params.mChannel.isEmpty()) {
            urlString += "&channel=" + params.mChannel;
        }

        return urlString;
    }

    /** Object holding information about the seed download parameters. */
    public static class SeedFetchParameters {
        private @VariationsPlatform int mPlatform;
        private String mRestrictMode;
        private String mMilestone;
        private String mChannel;
        private boolean mIsFastFetchMode;

        // This is added as a convenience for using Mockito.
        @Override
        public boolean equals(final Object obj) {
            if (obj == null) return false;
            if (obj.getClass() != this.getClass()) return false;
            if (!(obj instanceof SeedFetchParameters)) return false;
            SeedFetchParameters castObj = (SeedFetchParameters) obj;

            return getPlatform() == castObj.getPlatform()
                    && getIsFastFetchMode() == castObj.getIsFastFetchMode()
                    && Objects.equals(getMilestone(), castObj.getMilestone())
                    && Objects.equals(getRestrictMode(), castObj.getRestrictMode())
                    && Objects.equals(getChannel(), castObj.getChannel());
        }

        @Override
        public int hashCode() {
            return Objects.hash(mPlatform, mRestrictMode, mMilestone, mChannel, mIsFastFetchMode);
        }

        private SeedFetchParameters(
                @VariationsPlatform int platform,
                String restrictMode,
                String milestone,
                String channel,
                boolean isFastFetchMode) {
            this.mPlatform = platform;
            this.mRestrictMode = restrictMode;
            this.mMilestone = milestone;
            this.mChannel = channel;
            this.mIsFastFetchMode = isFastFetchMode;
        }

        /** Builder class for {@link SeedFetchParameters}. */
        public static class Builder {
            private @VariationsPlatform int mPlatform;
            private String mRestrictMode;
            private String mMilestone;
            private String mChannel;
            private boolean mIsFastFetchMode;

            private Builder() {
                this.mPlatform = VariationsPlatform.ANDROID;
                this.mIsFastFetchMode = false;
            }

            public SeedFetchParameters build() {
                return new SeedFetchParameters(
                        mPlatform, mRestrictMode, mMilestone, mChannel, mIsFastFetchMode);
            }

            public static Builder newBuilder() {
                return new Builder();
            }

            public Builder setPlatform(@VariationsPlatform int platform) {
                this.mPlatform = platform;
                return this;
            }

            public Builder setRestrictMode(String restrictMode) {
                this.mRestrictMode = restrictMode;
                return this;
            }

            public Builder setMilestone(String milestone) {
                this.mMilestone = milestone;
                return this;
            }

            public Builder setChannel(String channel) {
                this.mChannel = channel;
                return this;
            }

            public Builder setIsFastFetchMode(Boolean isFastFetchMode) {
                this.mIsFastFetchMode = isFastFetchMode;
                return this;
            }
        }

        // Getters
        public @VariationsPlatform int getPlatform() {
            return mPlatform;
        }

        public String getRestrictMode() {
            return mRestrictMode;
        }

        public String getMilestone() {
            return mMilestone;
        }

        public String getChannel() {
            return mChannel;
        }

        public boolean getIsFastFetchMode() {
            return mIsFastFetchMode;
        }
    }

    /** Object holding information about the status of a seed download attempt. */
    public static class SeedFetchInfo {
        // The result of the download, containing either an HTTP status code or a negative
        // value representing a specific error. This value is suitable for recording to the
        // "Variations.FirstRun.SeedFetchResult" histogram. This is equal to
        // HttpURLConnection.HTTP_OK if and only if the fetch succeeded.
        public int seedFetchResult;

        // Information about the seed that was downloaded. Null if the download failed.
        public SeedInfo seedInfo;
    }

    /** Object holding the seed data and related fields retrieved from HTTP headers. */
    public static class SeedInfo {
        // If you add fields, see VariationsTestUtils.
        public String signature;
        public String country;
        // Date according to the Variations server in milliseconds since UNIX epoch GMT.
        public long date;
        public boolean isGzipCompressed;
        public byte[] seedData;

        // Applies the {@code deltaPatch} to {@code previousSeedData} and returns the uncompressed
        // seed.
        @VisibleForTesting
        @SuppressWarnings("IgnoredPureGetter")
        public static byte[] resolveDeltaCompression(
                byte[] deltaPatch, byte[] previousSeedData, boolean isGzipCompressed)
                throws DeltaPatchException {
            assert CommandLine.getInstance()
                            .hasSwitch(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
                    : "Delta compression not enabled";
            try {
                if (isGzipCompressed) {
                    // Resolve gzip compression before applying the delta patch.
                    deltaPatch = VariationsCompressionUtils.gzipUncompress(deltaPatch);
                }
                byte[] patchedSeed =
                        VariationsCompressionUtils.applyDeltaPatch(previousSeedData, deltaPatch);

                // Parse seed to make sure the decompression was successful.

                return patchedSeed;
            } catch (IOException e) {
                Log.w(TAG, "Failed to resolve delta patch.", e);
                throw new DeltaPatchException("Error resolving delta patch");
            }
        }

        // Resolves the gzip compression of {@code seedData} and returns the byte array.
        @VisibleForTesting
        public byte[] getVariationsSeedBytes() throws IOException {
            if (this.isGzipCompressed) {
                return VariationsCompressionUtils.gzipUncompress(this.seedData);
            }
            return this.seedData;
        }

        // Returns the parsed VariationsSeed from {@code seedData}, if gzip compressed, resolves
        // gzip compression before parsing. Returns null if uncompressing or parsing fails.
        @Nullable
        @VisibleForTesting
        public VariationsSeed getParsedVariationsSeed() {
            if (this.seedData == null) {
                return null;
            }
            VariationsSeed proto;
            try {
                proto = VariationsSeed.parseFrom(getVariationsSeedBytes());
            } catch (InvalidProtocolBufferException e) {
                Log.w(TAG, "InvalidProtocolBufferException when parsing the variations seed.", e);
                return null;
            } catch (IOException e) {
                Log.w(TAG, "IOException when un-gzipping the variations seed.", e);
                return null;
            }
            return proto;
        }

        @Override
        public String toString() {
            if (BuildConfig.ENABLE_ASSERTS) {
                return "SeedInfo{signature=\""
                        + signature
                        + "\" country=\""
                        + country
                        + "\" date=\""
                        + date
                        + "\" isGzipCompressed="
                        + isGzipCompressed
                        + " seedData="
                        + Arrays.toString(seedData);
            }
            return super.toString();
        }
    }

    /**
     * Fetch the first run variations seed.
     *
     * @param restrictMode The restrict mode parameter to pass to the server via a URL param.
     * @param milestone The milestone parameter to pass to the server via a URL param.
     * @param channel The channel parameter to pass to the server via a URL param.
     */
    public void fetchSeed(String restrictMode, String milestone, String channel) {
        assert !ThreadUtils.runningOnUiThread();
        // Prevent multiple simultaneous fetches
        synchronized (sLock) {
            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            // Early return if an attempt has already been made to fetch the seed, even if it
            // failed. Only attempt to get the initial Java seed once, since a failure probably
            // indicates a network problem that is unlikely to be resolved by a second attempt.
            // Note that VariationsSeedBridge.hasNativePref() is a pure Java function, reading an
            // Android preference that is set when the seed is fetched by the native code.
            if (prefs.getBoolean(VARIATIONS_INITIALIZED_PREF, false)
                    || VariationsSeedBridge.hasNativePref()) {
                return;
            }

            SeedFetchParameters params =
                    SeedFetchParameters.Builder.newBuilder()
                            .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                            .setRestrictMode(null)
                            .setMilestone(milestone)
                            .setChannel(channel)
                            .build();
            SeedFetchInfo fetchInfo = downloadContent(params, null);
            if (fetchInfo.seedInfo != null) {
                SeedInfo info = fetchInfo.seedInfo;
                VariationsSeedBridge.setVariationsFirstRunSeed(
                        info.seedData,
                        info.signature,
                        info.country,
                        info.date,
                        info.isGzipCompressed);
            }
            // VARIATIONS_INITIALIZED_PREF should still be set to true when exceptions occur
            prefs.edit().putBoolean(VARIATIONS_INITIALIZED_PREF, true).apply();
        }
    }

    private void recordFetchResultOrCode(int resultOrCode) {
        RecordHistogram.recordSparseHistogram(SEED_FETCH_RESULT_HISTOGRAM, resultOrCode);
    }

    private void recordRequestedAndReceivedDeltaCompression(boolean requested, boolean received) {
        if (requested && received) {
            RecordHistogram.recordEnumeratedHistogram(
                    SEED_FETCH_DELTA_COMPRESSION,
                    DeltaCompression.REQUESTED_RECEIVED,
                    DeltaCompression.NUM_ENTRIES);
        } else if (requested && !received) {
            RecordHistogram.recordEnumeratedHistogram(
                    SEED_FETCH_DELTA_COMPRESSION,
                    DeltaCompression.REQUESTED_NOT_RECEIVED,
                    DeltaCompression.NUM_ENTRIES);
        } else if (!requested && received) {
            RecordHistogram.recordEnumeratedHistogram(
                    SEED_FETCH_DELTA_COMPRESSION,
                    DeltaCompression.NOT_REQUESTED_RECEIVED,
                    DeltaCompression.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    SEED_FETCH_DELTA_COMPRESSION,
                    DeltaCompression.NOT_REQUESTED_NOT_RECEIVED,
                    DeltaCompression.NUM_ENTRIES);
        }
    }

    private void recordSeedFetchTime(long timeDeltaMillis) {
        Log.i(TAG, "Fetched first run seed in " + timeDeltaMillis + " ms");
        RecordHistogram.recordTimesHistogram("Variations.FirstRun.SeedFetchTime", timeDeltaMillis);
    }

    private void recordSeedConnectTime(long timeDeltaMillis) {
        RecordHistogram.recordTimesHistogram(
                "Variations.FirstRun.SeedConnectTime", timeDeltaMillis);
    }

    /**
     * Download the variations seed data with platform and restrictMode.
     *
     * @param currInfo optional currently saved seed info to set the `If-None-Match` header.
     * @return the object holds the request result and seed data with its related header fields.
     */
    public SeedFetchInfo downloadContent(SeedFetchParameters params, SeedInfo currInfo) {
        SeedFetchInfo fetchInfo = new SeedFetchInfo();
        HttpURLConnection connection = null;
        try {
            long startTimeMillis = SystemClock.elapsedRealtime();
            connection = getServerConnection(params);
            connection.setReadTimeout(READ_TIMEOUT);
            connection.setConnectTimeout(REQUEST_TIMEOUT);
            connection.setDoInput(true);
            if (currInfo != null) {
                VariationsSeed currentVariationsSeed = currInfo.getParsedVariationsSeed();
                if (currentVariationsSeed != null) {
                    String serialNumber = currentVariationsSeed.getSerialNumber();
                    if (!serialNumber.isEmpty()) {
                        connection.setRequestProperty("If-None-Match", serialNumber);
                    }
                }
            }
            List<String> requestedInstanceManipulations = getAvailableInstanceManipulations();
            connection.setRequestProperty("A-IM", String.join(",", requestedInstanceManipulations));
            connection.connect();
            int responseCode = connection.getResponseCode();
            fetchInfo.seedFetchResult = responseCode;
            if (responseCode == HttpURLConnection.HTTP_OK) {
                recordSeedConnectTime(SystemClock.elapsedRealtime() - startTimeMillis);

                SeedInfo seedInfo = new SeedInfo();
                seedInfo.signature = getHeaderFieldOrEmpty(connection, "X-Seed-Signature");
                seedInfo.country = getHeaderFieldOrEmpty(connection, "X-Country");
                seedInfo.date = mDateTime.newDate().getTime();

                InstanceManipulations receivedIm =
                        VariationsCompressionUtils.getInstanceManipulations(
                                getHeaderFieldOrEmpty(connection, "IM"));

                // Log Delta-Compression before uncompressing data, as failure cases for that
                // are logged separately.
                recordRequestedAndReceivedDeltaCompression(
                        requestedInstanceManipulations.contains(
                                VariationsCompressionUtils.DELTA_COMPRESSION_HEADER),
                        receivedIm.isDeltaCompressed);

                byte[] seedData = getRawSeed(connection);
                boolean isGzipCompressed = receivedIm.isGzipCompressed;
                // Resolve the delta compression immediately as we only use the patched data.
                if (receivedIm.isDeltaCompressed) {
                    seedData =
                            SeedInfo.resolveDeltaCompression(
                                    seedData, currInfo.getVariationsSeedBytes(), isGzipCompressed);
                    isGzipCompressed = false;
                }
                // Ensure seed is gzip compressed.
                if (!isGzipCompressed) {
                    seedData = VariationsCompressionUtils.gzipCompress(seedData);
                    isGzipCompressed = true;
                }
                seedInfo.seedData = seedData;
                seedInfo.isGzipCompressed = isGzipCompressed;

                recordSeedFetchTime(SystemClock.elapsedRealtime() - startTimeMillis);
                fetchInfo.seedInfo = seedInfo;
            } else if (responseCode == HttpURLConnection.HTTP_NOT_MODIFIED) {
                // Update the seed date value in local state (used for expiry check on
                // next start up), since 304 is a successful response. Note that the
                // serial number included in the request is always that of the latest
                // seed, so it's appropriate to always modify the latest seed's date.
                fetchInfo.seedInfo = currInfo;
                fetchInfo.seedInfo.date = mDateTime.newDate().getTime();
            } else {
                String errorMsg = "Non-OK response code = " + responseCode;
                Log.w(TAG, errorMsg);
            }
        } catch (SocketTimeoutException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_TIMEOUT;
            Log.w(TAG, "SocketTimeoutException timeout when fetching variations seed.", e);
        } catch (UnknownHostException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION;
            Log.w(TAG, "UnknownHostException unknown host when fetching variations seed.", e);
        } catch (IOException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_IOEXCEPTION;
            Log.w(TAG, "IOException when fetching variations seed.", e);
        } catch (InvalidImHeaderException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_INVALID_IM_HEADER;
            Log.w(TAG, "InvalidImHeaderException when fetching variations seed.", e);
        } catch (DeltaPatchException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION;
            Log.w(TAG, "DeltaPatchException when fetching variations seed.", e);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            recordFetchResultOrCode(fetchInfo.seedFetchResult);
            return fetchInfo;
        }
    }

    private String getHeaderFieldOrEmpty(HttpURLConnection connection, String name) {
        String headerField = connection.getHeaderField(name);
        if (headerField == null) {
            return "";
        }
        return headerField.trim();
    }

    private byte[] getRawSeed(HttpURLConnection connection) throws IOException {
        InputStream inputStream = null;
        try {
            inputStream = connection.getInputStream();
            return FileUtils.readStream(inputStream);
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }
}

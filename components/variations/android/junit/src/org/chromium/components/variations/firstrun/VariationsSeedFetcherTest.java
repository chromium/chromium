// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.SharedPreferences;
import android.os.Looper;
import android.util.Base64;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.components.variations.VariationsCompressionUtils;
import org.chromium.components.variations.VariationsSeedOuterClass.VariationsSeed;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.DateTime;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedFetchInfo;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.util.Arrays;
import java.util.Date;
import java.util.List;

/** Tests for VariationsSeedFetcher */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VariationsSeedFetcherTest {
    private HttpURLConnection mConnection;
    private VariationsSeedFetcher mFetcher;
    private SharedPreferences mPrefs;

    private static final String sRestrict = "restricted";
    private static final String sMilestone = "64";
    private static final String sChannel = "dev";

    @Before
    public void setUp() throws IOException {
        // Pretend we are not on the UI thread, since the class we are testing is supposed to run
        // only on a background thread.
        ThreadUtils.setWillOverrideUiThread();
        ThreadUtils.setUiThread(mock(Looper.class));
        mFetcher = spy(VariationsSeedFetcher.get());
        mConnection = mock(HttpURLConnection.class);
        doReturn(mConnection)
                .when(mFetcher)
                .getServerConnection(
                        VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                                .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                                .setRestrictMode(sRestrict)
                                .setMilestone(sMilestone)
                                .setChannel(sChannel)
                                .build());
        mPrefs = ContextUtils.getAppSharedPreferences();
    }

    /** Test method for {@link VariationsSeedFetcher#fetchSeed()}. */
    @Test
    public void testFetchSeed() throws IOException {
        // Pretend we are on a background thread; set the UI thread looper to something other than
        // the current thread.

        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("X-Seed-Signature")).thenReturn("signature");
        when(mConnection.getHeaderField("X-Country")).thenReturn("Nowhere Land");
        when(mConnection.getHeaderField("IM")).thenReturn("gzip");
        when(mConnection.getInputStream())
                .thenReturn(new ByteArrayInputStream(ApiCompatibilityUtils.getBytesUtf8("1234")));
        doReturn(mConnection)
                .when(mFetcher)
                .getServerConnection(
                        VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                                .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                                .setMilestone(sMilestone)
                                .setChannel(sChannel)
                                // Restrict mode is null because fetchSeed sets this to null.
                                .setRestrictMode(null)
                                .build());

        long startTime = new Date().getTime();
        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);
        long endTime = new Date().getTime();

        assertThat(
                mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_SIGNATURE, ""),
                equalTo("signature"));
        assertThat(
                mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_COUNTRY, ""),
                equalTo("Nowhere Land"));
        long seedDate = mPrefs.getLong(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_DATE, 0);
        // We use *OrEqualTo comparisons here to account for when both points in time fall into the
        // same tick of the clock.
        assertThat(
                "Seed date should be after the test start time",
                seedDate,
                greaterThanOrEqualTo(startTime));
        assertThat(
                "Seed date should be before the test end time",
                seedDate,
                lessThanOrEqualTo(endTime));
        assertTrue(
                mPrefs.getBoolean(
                        VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED, false));
        assertThat(
                mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_BASE64, ""),
                equalTo(
                        Base64.encodeToString(
                                ApiCompatibilityUtils.getBytesUtf8("1234"), Base64.NO_WRAP)));
        assertEquals(
                "Should be logged as HTTP code",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        HttpURLConnection.HTTP_OK));
        assertEquals(
                "Should only log Variations.FirstRun.SeedFetchResult once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} to ensure delta-compression
     * result is correctly logged to UMA when delta compression is requested and received.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void downloadContentLogsRequestedAndReceivedDeltaCompression() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("x-bm");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should be logged as requested and received",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION,
                        VariationsSeedFetcher.DeltaCompression.REQUESTED_RECEIVED));
        assertEquals(
                "Should only log Variations.FirstRun.DeltaCompression once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} to ensure delta-compression
     * result is correctly logged to UMA when delta compression is requested but not received.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void downloadContentLogsRequestedAndNotReceivedDeltaCompression() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should be logged as requested but not received",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION,
                        VariationsSeedFetcher.DeltaCompression.REQUESTED_NOT_RECEIVED));
        assertEquals(
                "Should only log Variations.FirstRun.DeltaCompression once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} to ensure delta-compression
     * result is correctly logged to UMA when delta compression is requested but not received.
     */
    @Test
    public void downloadContentLogsNotRequestedButReceivedDeltaCompression() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("x-bm");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should be logged as not requested but received",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION,
                        VariationsSeedFetcher.DeltaCompression.NOT_REQUESTED_RECEIVED));
        assertEquals(
                "Should only log Variations.FirstRun.DeltaCompression once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} to ensure delta-compression
     * result is correctly logged to UMA when delta compression is requested but not received.
     */
    @Test
    public void downloadContentLogsNotRequestedNotReceivedDeltaCompression() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should be logged as not requested and not received",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION,
                        VariationsSeedFetcher.DeltaCompression.NOT_REQUESTED_NOT_RECEIVED));
        assertEquals(
                "Should only log Variations.FirstRun.DeltaCompression once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} to ensure delta-compression
     * result is not logged to UMA when return code is HTTP_NOT_MODIFIED.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void downloadContentNotLogsNotModifiedResponse() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_NOT_MODIFIED);
        when(mConnection.getHeaderField("IM")).thenReturn("x-bm");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should not log Variations.FirstRun.DeltaCompression",
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_DELTA_COMPRESSION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} when no fetch is needed as
     * If-None-Match header matches.
     */
    @Test
    public void downloadContentNotModified() throws IOException {
        // Pretend we are on a background thread; set the UI thread looper to something other than
        // the current thread.
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_NOT_MODIFIED);

        SeedInfo curSeedInfo = new SeedInfo();
        curSeedInfo.signature = "";
        curSeedInfo.country = "US";
        curSeedInfo.isGzipCompressed = false;
        Date lastSeedDate = new Date();
        lastSeedDate.setTime(12345L);
        curSeedInfo.date = lastSeedDate.getTime();

        final Date date = mock(Date.class);
        when(date.getTime()).thenReturn(67890L);
        final DateTime dt = mock(DateTime.class);
        when(dt.newDate()).thenReturn(date);
        mFetcher.setDateTime(dt);

        VariationsSeed seed =
                VariationsSeed.newBuilder()
                        .setVersion("V")
                        .setSerialNumber("savedSerialNumber")
                        .build();
        curSeedInfo.seedData = seed.toByteArray();

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, curSeedInfo);

        assertEquals(seedFetchInfo.seedFetchResult, HttpURLConnection.HTTP_NOT_MODIFIED);

        SeedInfo updatedSeedInfo = seedFetchInfo.seedInfo;
        assertEquals(curSeedInfo.signature, updatedSeedInfo.signature);
        assertEquals(curSeedInfo.country, updatedSeedInfo.country);
        assertEquals(curSeedInfo.isGzipCompressed, updatedSeedInfo.isGzipCompressed);
        assertEquals(67890L, updatedSeedInfo.date);
        Arrays.equals(curSeedInfo.seedData, updatedSeedInfo.seedData);

        assertEquals(curSeedInfo.getParsedVariationsSeed().getSerialNumber(), "savedSerialNumber");
    }

    /** Test method for {@link VariationsSeedFetcher#downloadContent()} when IM-header is invalid. */
    @Test
    public void testDownloadContent_invalidImHeader() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("gzip,x-bm");
        when(mConnection.getInputStream())
                .thenReturn(new ByteArrayInputStream(ApiCompatibilityUtils.getBytesUtf8("1234")));

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, /* currInfo= */ null);

        assertEquals(
                "Should be logged as InvalidImHeader",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_INVALID_IM_HEADER));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} when IM-header is valid, but
     * delta patch fails.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void testDownloadContent_failedDeltaCompression() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("x-bm,gzip");
        when(mConnection.getInputStream())
                .thenReturn(
                        new ByteArrayInputStream(
                                ApiCompatibilityUtils.getBytesUtf8("bogusDeltaPatch")));

        SeedInfo seed = new SeedInfo();
        seed.seedData = ApiCompatibilityUtils.getBytesUtf8("bogusDeltaPatch");

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo seedFetchInfo = mFetcher.downloadContent(params, seed);

        assertEquals(
                "Should not be logged as invalidImHeader",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_INVALID_IM_HEADER));
        assertEquals(
                "Should be logged as SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} when IM-header is valid, and
     * delta patch succeeds.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void testDownloadContent_success() throws IOException {
        String mockSignature = "bogus seed signature";
        String mockCountry = "GB";
        String base64Delta =
                "KgooMjRkM2EzN2UwMWJlYjlmMDVmMzIzOGI1MzVmNzA4NWZmZWViODc0MAAqW+4BkgEKH1VN"
                        + "QS1Vbmlmb3JtaXR5LVRyaWFsLTIwLVBlcmNlbnQYgOOFwAU4AUIHZGVmYXVsdEoRCghncm91"
                        + "cF8wMRABGKO2yQFKEQoIZ3JvdXBfMDIQARiktskBShEKCGdyb3VwXzAzEAEYpbbJAUoRCghn"
                        + "cm91cF8wNBABGKa2yQFKEAoHZGVmYXVsdBABGKK2yQFgARJYCh9VTUEtVW5pZm9ybWl0eS1U"
                        + "cmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDwoLbm9uX2RlZmF1bHQQAUoLCgdk"
                        + "ZWZhdWx0EAFSBCgAKAFgAQ==";

        String base64BeforeSeed =
                "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
                        + "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
                        + "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
                        + "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
                        + "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
                        + "CgdkZWZhdWx0EAFgAQ==";

        String base64ExpectedSeedData =
                "CigyNGQzYTM3ZTAxYmViOWYwNWYzMjM4YjUzNWY3MDg1ZmZlZWI4NzQwElgKDFVNQVN0YWJp"
                        + "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
                        + "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEpIBCh9VTUEtVW5pZm9ybWl0eS1UcmlhbC0yMC1Q"
                        + "ZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKEQoIZ3JvdXBfMDEQARijtskBShEKCGdyb3VwXzAy"
                        + "EAEYpLbJAUoRCghncm91cF8wMxABGKW2yQFKEQoIZ3JvdXBfMDQQARimtskBShAKB2RlZmF1"
                        + "bHQQARiitskBYAESWAofVU1BLVVuaWZvcm1pdHktVHJpYWwtNTAtUGVyY2VudBiA44XABTgB"
                        + "QgdkZWZhdWx0Sg8KC25vbl9kZWZhdWx0EAFKCwoHZGVmYXVsdBABUgQoACgBYAE=";

        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_OK);
        when(mConnection.getHeaderField("IM")).thenReturn("x-bm,gzip");
        when(mConnection.getHeaderField("X-Seed-Signature")).thenReturn(mockSignature);
        when(mConnection.getHeaderField("X-Country")).thenReturn(mockCountry);
        when(mConnection.getInputStream())
                .thenReturn(
                        new ByteArrayInputStream(
                                VariationsCompressionUtils.gzipCompress(
                                        Base64.decode(base64Delta, Base64.NO_WRAP))));

        SeedInfo seed = new SeedInfo();
        seed.seedData = Base64.decode(base64BeforeSeed, Base64.NO_WRAP);

        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        SeedFetchInfo fetchInfo = mFetcher.downloadContent(params, seed);

        // Check the counters.
        assertEquals(
                "Should not be logged as invalidImHeader",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_INVALID_IM_HEADER));
        assertEquals(
                "Should not be logged as SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_DELTA_PATCH_EXCEPTION));
        assertEquals(
                "Should be logged as HTTP code",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        HttpURLConnection.HTTP_OK));

        // Check the returned SeedInfo.
        assertEquals(
                "Delta patched seed data should result in expectedSeedData",
                base64ExpectedSeedData,
                Base64.encodeToString(
                        VariationsCompressionUtils.gzipUncompress(fetchInfo.seedInfo.seedData),
                        Base64.NO_WRAP));
        assertTrue(fetchInfo.seedInfo.isGzipCompressed);
        assertEquals(mockSignature, fetchInfo.seedInfo.signature);
        assertEquals(mockCountry, fetchInfo.seedInfo.country);
    }

    /** Test method for {@link VariationsSeedFetcher#fetchSeed()} when no fetch is needed */
    @Test
    public void testFetchSeed_noFetchNeeded() throws IOException {
        mPrefs.edit().putBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, true).apply();

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        verify(mConnection, never()).connect();
        assertEquals(
                "Should not log Variations.FirstRun.SeedFetchResult if no fetch needed",
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /** Test method for {@link VariationsSeedFetcher#fetchSeed()} with a bad response */
    @Test
    public void testFetchSeed_badResponse() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_NOT_FOUND);
        doReturn(mConnection)
                .when(mFetcher)
                .getServerConnection(
                        VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                                .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                                .setMilestone(sMilestone)
                                .setChannel(sChannel)
                                .build());

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        assertTrue(mPrefs.getBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, false));
        assertFalse(VariationsSeedBridge.hasJavaPref());
        assertEquals(
                "Should be logged as HTTP code",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        HttpURLConnection.HTTP_NOT_FOUND));
        assertEquals(
                "Should only log Variations.FirstRun.SeedFetchResult once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /** Test method for {@link VariationsSeedFetcher#fetchSeed()} with an exception when connecting */
    @Test
    public void testFetchSeed_IOException() throws IOException {
        doThrow(new IOException()).when(mConnection).connect();
        doReturn(mConnection)
                .when(mFetcher)
                .getServerConnection(
                        VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                                .setPlatform(VariationsSeedFetcher.VariationsPlatform.ANDROID)
                                .setMilestone(sMilestone)
                                .setChannel(sChannel)
                                .build());

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        assertTrue(mPrefs.getBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, false));
        assertFalse(VariationsSeedBridge.hasJavaPref());
        assertEquals(
                "Should be logged as IOException",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_IOEXCEPTION));
        assertEquals(
                "Should only log Variations.FirstRun.SeedFetchResult once",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /** Test method for {@link VariationsSeedFetcher#getConnectionString()} has URl params. */
    @Test
    public void testGetConnectionString_HasParams() {
        @VariationsSeedFetcher.VariationsPlatform
        int platform = VariationsSeedFetcher.VariationsPlatform.ANDROID;
        VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        String urlString = mFetcher.getConnectionString(params);

        // Has the params.
        assertTrue(urlString, urlString.contains("restrict"));
        assertTrue(urlString, urlString.contains("osname"));
        assertTrue(urlString, urlString.contains("milestone"));
        assertTrue(urlString, urlString.contains("channel"));

        // Has the param values.
        assertTrue(urlString, urlString.contains(sRestrict));
        assertTrue(urlString, urlString.contains(sMilestone));
        assertTrue(urlString, urlString.contains(sChannel));

        params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode("")
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        urlString = mFetcher.getConnectionString(params);
        assertFalse(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertTrue(urlString.contains("milestone"));
        assertTrue(urlString.contains("channel"));

        params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone("")
                        .setChannel(sChannel)
                        .build();
        urlString = mFetcher.getConnectionString(params);
        assertTrue(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertFalse(urlString.contains("milestone"));
        assertTrue(urlString.contains("channel"));

        params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel("")
                        .build();
        urlString = mFetcher.getConnectionString(params);
        assertTrue(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertTrue(urlString.contains("milestone"));
        assertFalse(urlString.contains("channel"));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#getConnectionString()} has non-fast-finch URl
     * params.
     */
    @Test
    public void testGetConnectionString_DoesNotHaveFastFinchParam() {
        @VariationsSeedFetcher.VariationsPlatform
        int platform = VariationsSeedFetcher.VariationsPlatform.ANDROID;
        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        String urlString = mFetcher.getConnectionString(params);
        assertFalse(urlString.contains("fastfinch"));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#getConnectionString()} has fast-finch URl
     * params.
     */
    @Test
    public void testGetConnectionString_HasFastFinchParam() {
        @VariationsSeedFetcher.VariationsPlatform
        int platform = VariationsSeedFetcher.VariationsPlatform.ANDROID;
        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .setIsFastFetchMode(true)
                        .build();
        String urlString = mFetcher.getConnectionString(params);
        assertTrue(urlString.contains("fastfinch"));
    }

    /**
     * Test method to make sure {@link VariationsSeedFetcher#getConnectionString()} honors the
     * "--fake-variations-channel" switch.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.FAKE_VARIATIONS_CHANNEL + "=stable")
    public void testGetConnectionString_HonorsChannelCommandlineSwitch() {
        @VariationsSeedFetcher.VariationsPlatform
        int platform = VariationsSeedFetcher.VariationsPlatform.ANDROID;
        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        String urlString = mFetcher.getConnectionString(params);

        // The URL should have a channel param.
        assertTrue(urlString, urlString.contains("channel"));

        // The channel param should be overridden by commandline.
        assertTrue(urlString, urlString.contains("stable"));
    }

    /**
     * Test method to make sure {@link VariationsSeedFetcher#getConnectionString()} honors the
     * "--variations-server-url" switch.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.VARIATIONS_SERVER_URL + "=http://localhost:8080/seed")
    public void testGetConnectionString_HonorsServerUrlCommandlineSwitch() {
        @VariationsSeedFetcher.VariationsPlatform
        int platform = VariationsSeedFetcher.VariationsPlatform.ANDROID;
        final VariationsSeedFetcher.SeedFetchParameters params =
                VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                        .setPlatform(platform)
                        .setRestrictMode(sRestrict)
                        .setMilestone(sMilestone)
                        .setChannel(sChannel)
                        .build();
        String urlString = mFetcher.getConnectionString(params);

        // The URL should start with the variations server URL passed as a switch.
        assertTrue(urlString, urlString.startsWith("http://localhost:8080/seed"));
    }

    /**
     * Test method to make sure {@link VariationsSeedFetcher#getAvailableInstanceManipulations()}
     * honors the "--enable-finch-seed-delta-compression" switch.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION)
    public void testGetServerConnection_CommandLineEnableDeltaCompression() throws IOException {
        List<String> compression = mFetcher.getAvailableInstanceManipulations();
        assertEquals(Arrays.asList("x-bm", "gzip"), compression);
    }

    /**
     * Test method to make sure {@link VariationsSeedFetcher#getAvailableInstanceManipulations()}
     * honors the absence of the "--enable-finch-seed-delta-compression" switch.
     */
    @Test
    public void testGetServerConnection_CommandLineDisabledDeltaCompression() throws IOException {
        List<String> compression = mFetcher.getAvailableInstanceManipulations();
        assertEquals(Arrays.asList("gzip"), compression);
    }
}

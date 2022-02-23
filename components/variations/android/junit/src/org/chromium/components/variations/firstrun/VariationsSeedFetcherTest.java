// Copyright 2017 The Chromium Authors. All rights reserved.
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
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
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

/**
 * Tests for VariationsSeedFetcher
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class VariationsSeedFetcherTest {
    private HttpURLConnection mConnection;
    private VariationsSeedFetcher mFetcher;
    private SharedPreferences mPrefs;
    private Looper mOrigLooper;

    private static final String sRestrict = "restricted";
    private static final String sMilestone = "64";
    private static final String sChannel = "dev";

    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();

    @Before
    public void setUp() throws IOException {
        // Pretend we are not on the UI thread, since the class we are testing is supposed to run
        // only on a background thread.
        Handler handler = ThreadUtils.getUiThreadHandler();
        if (handler != null) mOrigLooper = handler.getLooper();
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(true);
        ThreadUtils.setUiThread(mock(Looper.class));
        mFetcher = spy(VariationsSeedFetcher.get());
        mConnection = mock(HttpURLConnection.class);
        doReturn(mConnection)
                .when(mFetcher)
                .getServerConnection(VariationsSeedFetcher.VariationsPlatform.ANDROID, sRestrict,
                        sMilestone, sChannel);
        mPrefs = ContextUtils.getAppSharedPreferences();
        ShadowRecordHistogram.reset();
    }

    @After
    public void tearDown() {
        ThreadUtils.setUiThread(null);
        if (mOrigLooper != null) {
            ThreadUtils.setUiThread(mOrigLooper);
            mOrigLooper = null;
        }
        ThreadUtils.setWillOverrideUiThread(false);
    }

    /**
     * Test method for {@link VariationsSeedFetcher#fetchSeed()}.
     *
     * @throws IOException
     */
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

        long startTime = new Date().getTime();
        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);
        long endTime = new Date().getTime();

        assertThat(mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_SIGNATURE, ""),
                equalTo("signature"));
        assertThat(mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_COUNTRY, ""),
                equalTo("Nowhere Land"));
        long seedDate = mPrefs.getLong(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_DATE, 0);
        // We use *OrEqualTo comparisons here to account for when both points in time fall into the
        // same tick of the clock.
        assertThat("Seed date should be after the test start time", seedDate,
                greaterThanOrEqualTo(startTime));
        assertThat("Seed date should be before the test end time", seedDate,
                lessThanOrEqualTo(endTime));
        assertTrue(mPrefs.getBoolean(
                VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_IS_GZIP_COMPRESSED, false));
        assertThat(mPrefs.getString(VariationsSeedBridge.VARIATIONS_FIRST_RUN_SEED_BASE64, ""),
                equalTo(Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8("1234"), Base64.NO_WRAP)));
        assertEquals("Should be logged as HTTP code", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        HttpURLConnection.HTTP_OK));
        assertEquals("Should only log Variations.FirstRun.SeedFetchResult once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#downloadContent()} when no fetch is needed as
     * If-None-Match header matches.
     *
     * @throws IOException
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

        VariationsSeed seed = VariationsSeed.newBuilder()
                                      .setVersion("V")
                                      .setSerialNumber("savedSerialNumber")
                                      .build();
        curSeedInfo.seedData = seed.toByteArray();

        SeedFetchInfo seedFetchInfo =
                mFetcher.downloadContent(VariationsSeedFetcher.VariationsPlatform.ANDROID,
                        sRestrict, sMilestone, sChannel, curSeedInfo);

        assertEquals(seedFetchInfo.seedFetchResult, HttpURLConnection.HTTP_NOT_MODIFIED);

        SeedInfo updatedSeedInfo = seedFetchInfo.seedInfo;
        assertEquals(curSeedInfo.signature, updatedSeedInfo.signature);
        assertEquals(curSeedInfo.country, updatedSeedInfo.country);
        assertEquals(curSeedInfo.isGzipCompressed, updatedSeedInfo.isGzipCompressed);
        assertEquals(67890L, updatedSeedInfo.date);
        Arrays.equals(curSeedInfo.seedData, updatedSeedInfo.seedData);

        assertEquals(curSeedInfo.getParsedVariationsSeed().getSerialNumber(), "savedSerialNumber");
    }

    /**
     * Test method for {@link VariationsSeedFetcher#fetchSeed()} when no fetch is needed
     */
    @Test
    public void testFetchSeed_noFetchNeeded() throws IOException {
        mPrefs.edit().putBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, true).apply();

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        verify(mConnection, never()).connect();
        assertEquals("Should not log Variations.FirstRun.SeedFetchResult if no fetch needed", 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#fetchSeed()} with a bad response
     */
    @Test
    public void testFetchSeed_badResponse() throws IOException {
        when(mConnection.getResponseCode()).thenReturn(HttpURLConnection.HTTP_NOT_FOUND);

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        assertTrue(mPrefs.getBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, false));
        assertFalse(VariationsSeedBridge.hasJavaPref());
        assertEquals("Should be logged as HTTP code", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        HttpURLConnection.HTTP_NOT_FOUND));
        assertEquals("Should only log Variations.FirstRun.SeedFetchResult once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#fetchSeed()} with an exception when connecting
     */
    @Test
    public void testFetchSeed_IOException() throws IOException {
        doThrow(new IOException()).when(mConnection).connect();

        mFetcher.fetchSeed(sRestrict, sMilestone, sChannel);

        assertTrue(mPrefs.getBoolean(VariationsSeedFetcher.VARIATIONS_INITIALIZED_PREF, false));
        assertFalse(VariationsSeedBridge.hasJavaPref());
        assertEquals("Should be logged as IOException", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM,
                        VariationsSeedFetcher.SEED_FETCH_RESULT_IOEXCEPTION));
        assertEquals("Should only log Variations.FirstRun.SeedFetchResult once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        VariationsSeedFetcher.SEED_FETCH_RESULT_HISTOGRAM));
    }

    /**
     * Test method for {@link VariationsSeedFetcher#getConnectionString()} has URl params.
     */
    @Test
    public void testGetConnectionString_HasParams() {
        String urlString = mFetcher.getConnectionString(
                VariationsSeedFetcher.VariationsPlatform.ANDROID, sRestrict, sMilestone, sChannel);

        // Has the params.
        assertTrue(urlString, urlString.contains("restrict"));
        assertTrue(urlString, urlString.contains("osname"));
        assertTrue(urlString, urlString.contains("milestone"));
        assertTrue(urlString, urlString.contains("channel"));

        // Has the param values.
        assertTrue(urlString, urlString.contains(sRestrict));
        assertTrue(urlString, urlString.contains(sMilestone));
        assertTrue(urlString, urlString.contains(sChannel));

        urlString = mFetcher.getConnectionString(
                VariationsSeedFetcher.VariationsPlatform.ANDROID, "", sMilestone, sChannel);
        assertFalse(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertTrue(urlString.contains("milestone"));
        assertTrue(urlString.contains("channel"));

        urlString = mFetcher.getConnectionString(
                VariationsSeedFetcher.VariationsPlatform.ANDROID, sRestrict, "", sChannel);
        assertTrue(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertFalse(urlString.contains("milestone"));
        assertTrue(urlString.contains("channel"));

        urlString = mFetcher.getConnectionString(
                VariationsSeedFetcher.VariationsPlatform.ANDROID, sRestrict, sMilestone, "");
        assertTrue(urlString.contains("restrict"));
        assertTrue(urlString.contains("osname"));
        assertTrue(urlString.contains("milestone"));
        assertFalse(urlString.contains("channel"));
    }

    /**
     * Test method to make sure {@link VariationsSeedFetcher#getConnectionString()} honors the
     * "--fake-variations-channel" switch.
     */
    @Test
    @CommandLineFlags.Add(VariationsSwitches.FAKE_VARIATIONS_CHANNEL + "=stable")
    public void testGetConnectionString_HonorsChannelCommandlineSwitch() {
        String urlString = mFetcher.getConnectionString(
                VariationsSeedFetcher.VariationsPlatform.ANDROID, sRestrict, sMilestone, sChannel);

        // The URL should have a channel param.
        assertTrue(urlString, urlString.contains("channel"));

        // The channel param should be overridden by commandline.
        assertTrue(urlString, urlString.contains("stable"));
    }
}

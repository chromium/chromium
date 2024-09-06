// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.jni_zero.CalledByNative;
import org.junit.Assert;

import org.chromium.components.safe_browsing.SafeBrowsingApiBridge.UrlCheckTimeObserver;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to verify the JNI bridge. Invoked by native unit tests:
 * (components/safe_browsing/android/safe_browsing_api_handler_bridge_unittest.cc/).
 */
public class SafeBrowsingApiHandlerBridgeNativeUnitTestHelper {
    /**
     * A fake SafetyNetApiHandler which verifies the parameters of the overridden functions and
     * returns lookup result based on preset values.
     */
    public static class MockSafetyNetApiHandler implements SafetyNetApiHandler {
        private Observer mObserver;
        // See safe_browsing_handler_util.h --> JavaThreatTypes
        private static final int THREAT_TYPE_CSD_ALLOWLIST = 16;
        // The result that will be returned in {@link #isVerifyAppsEnabled(long)} or {@link
        // #enableVerifyApps(long)}.
        private static int sVerifyAppsResult = VerifyAppsResult.FAILED;

        // Maps to store preset values, keyed by uri.
        private static final Map<String, Boolean> sCsdAllowlistMap = new HashMap<>();

        @Override
        public boolean init(Observer result) {
            mObserver = result;
            return true;
        }

        @Override
        public boolean startAllowlistLookup(final String uri, int threatType) {
            Assert.assertTrue(threatType == THREAT_TYPE_CSD_ALLOWLIST);
            return Boolean.TRUE.equals(sCsdAllowlistMap.get(uri));
        }

        @Override
        public void isVerifyAppsEnabled(final long callbackId) {
            mObserver.onVerifyAppsEnabledDone(callbackId, sVerifyAppsResult);
        }

        @Override
        public void enableVerifyApps(final long callbackId) {
            mObserver.onVerifyAppsEnabledDone(callbackId, sVerifyAppsResult);
        }

        public static void tearDown() {
            sCsdAllowlistMap.clear();
        }

        public static void setCsdAllowlistMatch(String uri, boolean match) {
            sCsdAllowlistMap.put(uri, match);
        }

        public static void setVerifyAppsResult(int result) {
            sVerifyAppsResult = result;
        }
    }

    /**
     * A fake SafeBrowsingApiHandler which verifies the parameters of the overridden functions and
     * returns lookup result based on preset values.
     */
    public static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private Observer mObserver;

        // Mock time it takes for a lookup request to complete. This value is verified on the native
        // side.
        private static final long DEFAULT_CHECK_DELTA_MICROSECONDS = 15;

        // Maps to store preset values, keyed by uri.
        private static final Map<String, UrlCheckDoneValues> sPresetValuesMap = new HashMap<>();

        @Override
        public void setObserver(Observer observer) {
            mObserver = observer;
        }

        @Override
        public void startUriLookup(
                final long callbackId, String uri, int[] threatTypes, int protocol) {
            Assert.assertTrue(sPresetValuesMap.containsKey(uri));
            UrlCheckDoneValues presetValues = sPresetValuesMap.get(uri);
            int[] expectedThreatTypes = presetValues.mExpectedThreatTypes;
            Assert.assertNotNull(expectedThreatTypes);
            // The order of threatTypes doesn't matter.
            Arrays.sort(expectedThreatTypes);
            Arrays.sort(threatTypes);
            Assert.assertArrayEquals(threatTypes, expectedThreatTypes);
            Assert.assertEquals(protocol, presetValues.mExpectedProtocol);

            mObserver.onUrlCheckDone(
                    callbackId,
                    presetValues.mReturnedLookupResult,
                    presetValues.mReturnedThreatType,
                    presetValues.mReturnedThreatAttributes,
                    presetValues.mReturnedResponseStatus,
                    DEFAULT_CHECK_DELTA_MICROSECONDS);
        }

        public static void tearDown() {
            sPresetValuesMap.clear();
        }

        public static void setUrlCheckDoneValues(
                String uri,
                int[] expectedThreatTypes,
                int expectedProtocol,
                int returnedLookupResult,
                int returnedThreatType,
                int[] returnedThreatAttributes,
                int returnedResponseStatus) {
            Assert.assertFalse(sPresetValuesMap.containsKey(uri));
            sPresetValuesMap.put(
                    uri,
                    new UrlCheckDoneValues(
                            expectedThreatTypes,
                            expectedProtocol,
                            returnedLookupResult,
                            returnedThreatType,
                            returnedThreatAttributes,
                            returnedResponseStatus));
        }

        private static class UrlCheckDoneValues {
            public final int[] mExpectedThreatTypes;
            public final int mExpectedProtocol;
            public final int mReturnedLookupResult;
            public final int mReturnedThreatType;
            public final int[] mReturnedThreatAttributes;
            public final int mReturnedResponseStatus;

            private UrlCheckDoneValues(
                    int[] expectedThreatTypes,
                    int expectedProtocol,
                    int returnedLookupResult,
                    int returnedThreatType,
                    int[] returnedThreatAttributes,
                    int returnedResponseStatus) {
                mExpectedThreatTypes = expectedThreatTypes;
                mExpectedProtocol = expectedProtocol;
                mReturnedLookupResult = returnedLookupResult;
                mReturnedThreatType = returnedThreatType;
                mReturnedThreatAttributes = returnedThreatAttributes;
                mReturnedResponseStatus = returnedResponseStatus;
            }
        }
    }

    public static final MockUrlCheckTimeObserver sSafeBrowsingApiUrlCheckTimeObserver =
            new MockUrlCheckTimeObserver();

    public static class MockUrlCheckTimeObserver implements UrlCheckTimeObserver {
        private long mCapturedUrlCheckTimeDeltaMicros;
        private boolean mIsOnUrlCheckTimeCalled;

        @Override
        public void onUrlCheckTime(long urlCheckTimeDeltaMicros) {
            Assert.assertFalse(
                    "Url check time should only be logged once.", mIsOnUrlCheckTimeCalled);
            mCapturedUrlCheckTimeDeltaMicros = urlCheckTimeDeltaMicros;
            mIsOnUrlCheckTimeCalled = true;
        }

        public long getCapturedUrlCheckTimeDeltaMicros() {
            return mCapturedUrlCheckTimeDeltaMicros;
        }

        public void tearDown() {
            mCapturedUrlCheckTimeDeltaMicros = 0;
            mIsOnUrlCheckTimeCalled = false;
        }
    }

    @CalledByNative
    static void setUp() {
        SafeBrowsingApiBridge.setSafetyNetApiHandler(new MockSafetyNetApiHandler());
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        SafeBrowsingApiBridge.setOneTimeSafeBrowsingApiUrlCheckObserver(
                sSafeBrowsingApiUrlCheckTimeObserver);
    }

    @CalledByNative
    static void tearDown() {
        MockSafetyNetApiHandler.tearDown();
        MockSafeBrowsingApiHandler.tearDown();
        sSafeBrowsingApiUrlCheckTimeObserver.tearDown();
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @CalledByNative
    static void setCsdAllowlistMatch(String uri, boolean match) {
        MockSafetyNetApiHandler.setCsdAllowlistMatch(uri, match);
    }

    @CalledByNative
    static void setSafeBrowsingApiHandlerResponse(
            String uri,
            int[] expectedThreatTypes,
            int expectedProtocol,
            int returnedLookupResult,
            int returnedThreatType,
            int[] returnedThreatAttributes,
            int returnedResponseStatus) {
        MockSafeBrowsingApiHandler.setUrlCheckDoneValues(
                uri,
                expectedThreatTypes,
                expectedProtocol,
                returnedLookupResult,
                returnedThreatType,
                returnedThreatAttributes,
                returnedResponseStatus);
    }

    @CalledByNative
    static long getSafeBrowsingApiUrlCheckTimeObserverResult() {
        return sSafeBrowsingApiUrlCheckTimeObserver.getCapturedUrlCheckTimeDeltaMicros();
    }

    @CalledByNative
    static void setVerifyAppsResult(int result) {
        MockSafetyNetApiHandler.setVerifyAppsResult(result);
    }
}

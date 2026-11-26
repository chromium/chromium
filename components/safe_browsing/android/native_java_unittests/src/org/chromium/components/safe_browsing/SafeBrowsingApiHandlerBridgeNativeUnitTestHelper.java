// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.jni_zero.CalledByNative;
import org.junit.Assert;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge.UrlCheckTimeObserver;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to verify the JNI bridge. Invoked by native unit tests:
 * (components/safe_browsing/android/safe_browsing_api_handler_bridge_unittest.cc/).
 */
public class SafeBrowsingApiHandlerBridgeNativeUnitTestHelper {
    // The static JNI entry points share one set of mock instances per gtest fixture.
    private static SafeBrowsingApiHandlerBridgeNativeUnitTestHelper sInstance;

    private final MockSafetyNetApiHandler mSafetyNetHandler = new MockSafetyNetApiHandler();
    private final MockSafeBrowsingApiHandler mSafeBrowsingHandler =
            new MockSafeBrowsingApiHandler();
    private final MockUrlCheckTimeObserver mUrlCheckTimeObserver = new MockUrlCheckTimeObserver();

    /**
     * A fake SafetyNetApiHandler which verifies the parameters of the overridden functions and
     * returns lookup result based on preset values.
     */
    private static class MockSafetyNetApiHandler implements SafetyNetApiHandler {
        private SafetyNetApiHandler.Observer mObserver;
        // See safe_browsing_api_handler_util.h --> SafetyNetJavaThreatType
        private static final int THREAT_TYPE_CSD_DOWNLOAD_ALLOWLIST = 9;
        private static final int THREAT_TYPE_CSD_ALLOWLIST = 16;
        // The result that will be returned in {@link #isVerifyAppsEnabled(long)} or {@link
        // #enableVerifyApps(long)}.
        private int mVerifyAppsResult = VerifyAppsResult.FAILED;

        // The results that will be returned in {@link #hasPotentiallyHarmfulApps(long)}}.
        private int mHarmfulAppsResult = HasHarmfulAppsResultStatus.FAILED;
        private int mHarmfulAppsNumOfApps;
        private int mHarmfulAppsStatusCode;

        // Maps to store preset values, keyed by uri.
        private final Map<String, Boolean> mCsdAllowlistMap = new HashMap<>();
        private final Map<String, Boolean> mCsdDownloadAllowlistMap = new HashMap<>();

        private @SafetyNetApiState int mSafetyNetApiInitializationState =
                SafetyNetApiState.INITIALIZED;

        // Number to insert into the mock value to return for the default getSafetyNetId() result.
        private int mGetSafetyNetIdCount;
        // A string to override the default mock result for getSafetyNetId().
        private String mSafetyNetIdResultOverride;

        @Override
        public @SafetyNetApiHandler.SafetyNetApiState int initialize(
                SafetyNetApiHandler.Observer observer) {
            mObserver = observer;
            return mSafetyNetApiInitializationState;
        }

        @Override
        public boolean startAllowlistLookup(final String uri, int threatType) {
            if (threatType == THREAT_TYPE_CSD_ALLOWLIST) {
                return Boolean.TRUE.equals(mCsdAllowlistMap.get(uri));
            }
            if (threatType == THREAT_TYPE_CSD_DOWNLOAD_ALLOWLIST) {
                return Boolean.TRUE.equals(mCsdDownloadAllowlistMap.get(uri));
            }
            assert false : "Unsupported threatType";
            return false;
        }

        @Override
        public void isVerifyAppsEnabled(final long callbackId) {
            mObserver.onVerifyAppsEnabledDone(callbackId, mVerifyAppsResult);
        }

        @Override
        public void enableVerifyApps(final long callbackId) {
            mObserver.onVerifyAppsEnabledDone(callbackId, mVerifyAppsResult);
        }

        @Override
        public void hasPotentiallyHarmfulApps(final long callbackId) {
            mObserver.onHasHarmfulAppsDone(
                    callbackId, mHarmfulAppsResult, mHarmfulAppsNumOfApps, mHarmfulAppsStatusCode);
        }

        @Override
        public void getSafetyNetId() {
            assert mSafetyNetApiInitializationState
                    == SafetyNetApiHandler.SafetyNetApiState.INITIALIZED_FIRST_PARTY;
            String result = mSafetyNetIdResultOverride;
            if (result == null) {
                result = String.format("safety-net-id-%d", mGetSafetyNetIdCount++);
            }
            mObserver.onGetSafetyNetIdDone(result);
        }
    }

    /**
     * A fake SafeBrowsingApiHandler which verifies the parameters of the overridden functions and
     * returns lookup result based on preset values.
     */
    private static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private SafeBrowsingApiHandler.Observer mObserver;

        // Mock time it takes for a lookup request to complete. This value is verified on the native
        // side.
        private static final long DEFAULT_CHECK_DELTA_MICROSECONDS = 15;

        // Maps to store preset values, keyed by uri.
        private final Map<String, UrlCheckDoneValues> mPresetValuesMap = new HashMap<>();

        @Override
        public void setObserver(SafeBrowsingApiHandler.Observer observer) {
            mObserver = observer;
        }

        @Override
        public void startUriLookup(
                final long callbackId, String uri, int[] threatTypes, int protocol) {
            Assert.assertTrue(mPresetValuesMap.containsKey(uri));
            UrlCheckDoneValues presetValues = mPresetValuesMap.get(uri);
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

        public void setUrlCheckDoneValues(
                String uri,
                int[] expectedThreatTypes,
                int expectedProtocol,
                int returnedLookupResult,
                int returnedThreatType,
                int[] returnedThreatAttributes,
                int returnedResponseStatus) {
            Assert.assertFalse(mPresetValuesMap.containsKey(uri));
            mPresetValuesMap.put(
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

    private static class MockUrlCheckTimeObserver implements UrlCheckTimeObserver {
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
    }

    @CalledByNative
    static void setUp() {
        if (ResettersForTesting.getState() == ResettersForTesting.State.NOT_ENABLED) {
            ResettersForTesting.enable();
        }
        // Treat each native fixture as a class scope, using the same lifecycle as Java tests.
        ResettersForTesting.beforeClassHooksWillExecute();
        sInstance = new SafeBrowsingApiHandlerBridgeNativeUnitTestHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SafeBrowsingApiBridge.setSafetyNetApiHandlerForTesting(
                            sInstance.mSafetyNetHandler);
                    SafeBrowsingApiBridge.setSafeBrowsingApiHandlerForTesting(
                            sInstance.mSafeBrowsingHandler);
                    SafeBrowsingApiBridge.setOneTimeSafeBrowsingApiUrlCheckObserverForTesting(
                            sInstance.mUrlCheckTimeObserver);
                });
    }

    @CalledByNative
    static void tearDown() {
        ResettersForTesting.afterClassHooksDidExecute();
        sInstance = null;
    }

    @CalledByNative
    static void setCsdAllowlistMatch(String uri, boolean match) {
        sInstance.mSafetyNetHandler.mCsdAllowlistMap.put(uri, match);
    }

    @CalledByNative
    static void setCsdDownloadAllowlistMatch(String uri, boolean match) {
        sInstance.mSafetyNetHandler.mCsdDownloadAllowlistMap.put(uri, match);
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
        sInstance.mSafeBrowsingHandler.setUrlCheckDoneValues(
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
        return sInstance.mUrlCheckTimeObserver.getCapturedUrlCheckTimeDeltaMicros();
    }

    @CalledByNative
    static void setVerifyAppsResult(int result) {
        sInstance.mSafetyNetHandler.mVerifyAppsResult = result;
    }

    @CalledByNative
    static void setHarmfulAppsResult(int result, int numOfApps, int statusCode) {
        sInstance.mSafetyNetHandler.mHarmfulAppsResult = result;
        sInstance.mSafetyNetHandler.mHarmfulAppsNumOfApps = numOfApps;
        sInstance.mSafetyNetHandler.mHarmfulAppsStatusCode = statusCode;
    }

    @CalledByNative
    static void setSafetyNetApiInitializationState(
            @SafetyNetApiHandler.SafetyNetApiState int state) {
        sInstance.mSafetyNetHandler.mSafetyNetApiInitializationState = state;
    }

    @CalledByNative
    static void setSafetyNetIdResultEmpty() {
        sInstance.mSafetyNetHandler.mSafetyNetIdResultOverride = "";
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;

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
        // The result that will be returned in {@link #startUriLookup(long, String, int[])}.
        private static int sResult = SafeBrowsingResult.SUCCESS;
        // Mock time it takes for a lookup request to complete. This value is verified on the native
        // side.
        private static final long DEFAULT_CHECK_DELTA_MS = 10;
        // See safe_browsing_handler_util.h --> JavaThreatTypes
        private static final int THREAT_TYPE_CSD_ALLOWLIST = 16;
        private static final int THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST = 17;

        // Maps to store preset values, keyed by uri.
        private static final Map<String, Boolean> sCsdAllowlistMap = new HashMap<>();
        private static final Map<String, Boolean> sHighConfidenceAllowlistMap = new HashMap<>();
        private static final Map<String, int[]> sThreatsOfInterestMap = new HashMap<>();
        private static final Map<String, String> sMetadataMap = new HashMap<>();

        @Override
        public boolean init(Observer result) {
            mObserver = result;
            return true;
        }

        @Override
        public void startUriLookup(final long callbackId, String uri, int[] threatsOfInterest) {
            if (sResult != SafeBrowsingResult.SUCCESS) {
                mObserver.onUrlCheckDone(callbackId, sResult, "{}", DEFAULT_CHECK_DELTA_MS);
                return;
            }
            int[] expectedThreatsOfInterest = sThreatsOfInterestMap.get(uri);
            Assert.assertNotNull(expectedThreatsOfInterest);
            // The order of threatsOfInterest doesn't matter.
            Arrays.sort(expectedThreatsOfInterest);
            Arrays.sort(threatsOfInterest);
            Assert.assertArrayEquals(threatsOfInterest, expectedThreatsOfInterest);
            Assert.assertTrue(sMetadataMap.containsKey(uri));
            mObserver.onUrlCheckDone(
                    callbackId, sResult, sMetadataMap.get(uri), DEFAULT_CHECK_DELTA_MS);
        }

        @Override
        public boolean startAllowlistLookup(final String uri, int threatType) {
            Assert.assertTrue(threatType == THREAT_TYPE_CSD_ALLOWLIST
                    || threatType == THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);
            if (threatType == THREAT_TYPE_CSD_ALLOWLIST) {
                return Boolean.TRUE.equals(sCsdAllowlistMap.get(uri));
            }
            return Boolean.TRUE.equals(sHighConfidenceAllowlistMap.get(uri));
        }

        public static void setExpectedThreatsOfInterest(String uri, int[] threatOfInterests) {
            sThreatsOfInterestMap.put(uri, threatOfInterests);
        }

        public static void setMetadata(String uri, String metadata) {
            sMetadataMap.put(uri, metadata);
        }

        public static void setCsdAllowlistMatch(String uri, boolean match) {
            sCsdAllowlistMap.put(uri, match);
        }

        public static void setHighConfidenceAllowlistMatch(String uri, boolean match) {
            sHighConfidenceAllowlistMap.put(uri, match);
        }

        public static void setResult(int result) {
            sResult = result;
        }
    }

    @CalledByNative
    static void setUp() {
        SafeBrowsingApiBridge.setHandler(new MockSafetyNetApiHandler());
    }

    @CalledByNative
    static void tearDown() {
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @CalledByNative
    static void setExpectedThreatsOfInterest(String uri, int[] threatsOfInterest) {
        MockSafetyNetApiHandler.setExpectedThreatsOfInterest(uri, threatsOfInterest);
    }

    @CalledByNative
    static void setMetadata(String uri, String metadata) {
        MockSafetyNetApiHandler.setMetadata(uri, metadata);
    }

    @CalledByNative
    static void setCsdAllowlistMatch(String uri, boolean match) {
        MockSafetyNetApiHandler.setCsdAllowlistMatch(uri, match);
    }

    @CalledByNative
    static void setHighConfidenceAllowlistMatch(String uri, boolean match) {
        MockSafetyNetApiHandler.setHighConfidenceAllowlistMatch(uri, match);
    }

    @CalledByNative
    static void setResult(int result) {
        MockSafetyNetApiHandler.setResult(result);
    }
}

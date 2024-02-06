// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/* Utility class to be shared for ExperimentalOptions translation related tests. */
public class ExperimentalOptionsTranslationTestUtil {
    private ExperimentalOptionsTranslationTestUtil() {}

    static int toTelephoneKeyboardSequence(String string) {
        int length = string.length();
        if (length > 9) {
            return toTelephoneKeyboardSequence(string.substring(0, 5)) * 10000
                    + toTelephoneKeyboardSequence(string.substring(length - 3, length));
        }

        // This could be optimized a lot but little inefficiency in tests doesn't matter all that
        // much and readability benefits are quite significant.
        Map<String, Integer> charMap = new HashMap<>();
        charMap.put("abc", 2);
        charMap.put("def", 3);
        charMap.put("ghi", 4);
        charMap.put("jkl", 5);
        charMap.put("mno", 6);
        charMap.put("pqrs", 7);
        charMap.put("tuv", 8);
        charMap.put("xyz", 9);

        int result = 0;
        for (int i = 0; i < length; i++) {
            result *= 10;
            for (Map.Entry<String, Integer> mapping : charMap.entrySet()) {
                if (mapping.getKey()
                        .contains(string.substring(i, i + 1).toLowerCase(Locale.ROOT))) {
                    result += mapping.getValue();
                    break;
                }
            }
        }
        return result;
    }

    public static void assertJsonEquals(String expected, String actual) {
        try {
            JSONObject expectedJson = new JSONObject(expected);
            JSONObject actualJson = new JSONObject(actual);

            assertJsonEquals(expectedJson, actualJson, "");
        } catch (JSONException e) {
            throw new AssertionError(e);
        }
    }

    static void assertJsonEquals(JSONObject expected, JSONObject actual, String currentPath)
            throws JSONException {
        assertThat(jsonKeys(actual)).isEqualTo(jsonKeys(expected));

        for (String key : jsonKeys(expected)) {
            Object expectedValue = expected.get(key);
            Object actualValue = actual.get(key);
            if (expectedValue == actualValue) {
                continue;
            }
            String fullKey = currentPath.isEmpty() ? key : currentPath + "." + key;
            if (expectedValue instanceof JSONObject) {
                assertWithMessage("key is '" + fullKey + "'")
                        .that(actualValue)
                        .isInstanceOf(JSONObject.class);
                assertJsonEquals((JSONObject) expectedValue, (JSONObject) actualValue, fullKey);
            } else {
                assertWithMessage("key is '" + fullKey + "'")
                        .that(actualValue)
                        .isEqualTo(expectedValue);
            }
        }
    }

    static Set<String> jsonKeys(JSONObject json) throws JSONException {
        Set<String> result = new HashSet<>();

        Iterator<String> keys = json.keys();

        while (keys.hasNext()) {
            String key = keys.next();
            result.add(key);
        }

        return result;
    }

    // Mocks make life downstream miserable so use a custom mock-like class.
    public static class MockCronetBuilderImpl extends ICronetEngineBuilder {
        public ConnectionMigrationOptions mConnectionMigrationOptions;
        public String mEffectiveExperimentalOptions;

        private String mTempExperimentalOptions;

        private final boolean mSupportsConnectionMigrationConfigOption;

        static MockCronetBuilderImpl withNativeSetterSupport() {
            return new MockCronetBuilderImpl(true);
        }

        public static MockCronetBuilderImpl withoutNativeSetterSupport() {
            return new MockCronetBuilderImpl(false);
        }

        private MockCronetBuilderImpl(boolean supportsConnectionMigrationConfigOption) {
            this.mSupportsConnectionMigrationConfigOption = supportsConnectionMigrationConfigOption;
        }

        @Override
        public ICronetEngineBuilder addPublicKeyPins(
                String hostName,
                Set<byte[]> pinsSha256,
                boolean includeSubdomains,
                Date expirationDate) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableHttp2(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(
                boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableQuic(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableSdch(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setExperimentalOptions(String options) {
            mTempExperimentalOptions = options;
            return this;
        }

        @Override
        public ICronetEngineBuilder setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setStoragePath(String value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setUserAgent(String userAgent) {
            throw new UnsupportedOperationException();
        }

        @Override
        public String getDefaultUserAgent() {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setConnectionMigrationOptions(
                ConnectionMigrationOptions options) {
            mConnectionMigrationOptions = options;
            return this;
        }

        @Override
        public Set<Integer> getSupportedConfigOptions() {
            if (mSupportsConnectionMigrationConfigOption) {
                return Collections.singleton(ICronetEngineBuilder.CONNECTION_MIGRATION_OPTIONS);
            } else {
                return Collections.emptySet();
            }
        }

        @Override
        public ExperimentalCronetEngine build() {
            mEffectiveExperimentalOptions = mTempExperimentalOptions;
            return null;
        }
    }
}

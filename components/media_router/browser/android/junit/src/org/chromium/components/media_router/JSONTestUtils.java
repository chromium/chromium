// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mockito.ArgumentMatcher;

import java.util.Iterator;

/** Utilities for comparing JSON objects and strings. */
public class JSONTestUtils {
    private static final String ANY_PREFIX = "ANY_";

    private static boolean isPureJSONObjectEqual(JSONObject expected, JSONObject actual) {
        try {
            Iterator<?> iterKey = expected.keys();
            while (iterKey.hasNext()) {
                String key = (String) iterKey.next();
                if (key.startsWith(ANY_PREFIX)) {
                    if (!actual.has(key.substring(ANY_PREFIX.length()))) return false;
                } else {
                    if (!isJSONObjectEqual(expected.get(key), actual.get(key))) return false;
                }
            }
        } catch (JSONException e) {
            return false;
        }
        return true;
    }

    /** Returns whether two JSON arrays are equal. */
    public static boolean isJSONArrayEqual(JSONArray expected, JSONArray actual) {
        try {
            if (expected.length() != actual.length()) return false;
            for (int i = 0; i < expected.length(); i++) {
                if (!isJSONObjectEqual(expected.get(i), actual.get(i))) return false;
            }
        } catch (JSONException e) {
            return false;
        }
        return true;
    }

    /** Returns whether two JSON objects are equal. */
    public static boolean isJSONObjectEqual(Object expected, Object actual) {
        if (expected == null && actual == null) return true;
        if (expected == null || actual == null) return false;

        if (expected.getClass() == JSONArray.class) {
            if (actual.getClass() != JSONArray.class) return false;
            if (!isJSONArrayEqual((JSONArray) expected, (JSONArray) actual)) return false;
        } else if (expected.getClass() == JSONObject.class) {
            if (actual.getClass() != JSONObject.class) return false;
            if (!isPureJSONObjectEqual((JSONObject) expected, (JSONObject) actual)) return false;
        } else if (expected.getClass() == Double.class || actual.getClass() == Double.class) {
            if (getDoubleValue(expected) != getDoubleValue(actual)) return false;
        } else {
            if (!expected.equals(actual)) return false;
        }
        return true;
    }

    private static double getDoubleValue(Object object) {
        if (object.getClass() == Integer.class) {
            return ((Integer) object).doubleValue();
        } else {
            return ((Double) object).doubleValue();
        }
    }

    /** Matcher to determine whether a JSON object is equal to the expected one. */
    public static class JSONObjectLike implements ArgumentMatcher<JSONObject> {
        private final JSONObject mExpected;

        public JSONObjectLike(JSONObject expected) {
            mExpected = expected;
        }

        @Override
        public boolean matches(JSONObject actual) {
            return isJSONObjectEqual(mExpected, actual);
        }

        @Override
        public String toString() {
            return "(JSONObject) " + mExpected.toString();
        }
    }

    /** Matcher to determine whether a JSON string is equal to the expected one. */
    public static class JSONStringLike implements ArgumentMatcher<String> {
        private JSONObject mExpected;

        public JSONStringLike(JSONObject expected) {
            mExpected = expected;
        }

        @Override
        public boolean matches(String actual) {
            try {
                return isJSONObjectEqual(mExpected, new JSONObject(actual));
            } catch (JSONException e) {
                return false;
            }
        }

        @Override
        public String toString() {
            return "\"" + mExpected.toString() + "\"";
        }
    }
}

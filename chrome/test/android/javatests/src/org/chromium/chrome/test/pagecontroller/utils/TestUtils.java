// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import androidx.test.uiautomator.BySelector;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.mockito.ArgumentMatcher;

import java.lang.reflect.Field;
import java.util.List;
import java.util.regex.Pattern;

/** Test utility methods. */
public final class TestUtils {
    /**
     * Stub the findObject(s) methods of device and node.
     *
     * @param device Device whose findObject(s) calls will be stubbed.
     * @param root Root whose findObject(s) calls will be stubbed.
     * @param selector Parameter of the findObject(s) calls.
     * @param result Return value when findObject is called with the selector.
     * @param results Return value when findObjects is called with the selector.
     */
    public static void stubMocks(
            UiDevice device,
            UiObject2 root,
            BySelector selector,
            UiObject2 result,
            List<UiObject2> results) {
        when(device.findObject(selector)).thenReturn(result);
        when(device.findObjects(selector)).thenReturn(results);
        when(root.findObject(selector)).thenReturn(result);
        when(root.findObjects(selector)).thenReturn(results);
    }

    /**
     * Assert on the return values of the locator operations.
     *
     * @param device Device parameter of the locate(One|All) methods.
     * @param root Root parameter of the locate(One|All) methods.
     * @param locator IUi2Locator object upon which to exercise the locate(One|All) methods.
     * @param result Expected result of the locateOne methods.
     * @param results Expected result of the locateAll methods.
     */
    public static void assertLocatorResults(
            UiDevice device,
            UiObject2 root,
            IUi2Locator locator,
            UiObject2 result,
            List<UiObject2> results) {
        assertEquals(result, locator.locateOne(device));
        assertEquals(results, locator.locateAll(device));
        assertEquals(result, locator.locateOne(root));
        assertEquals(results, locator.locateAll(root));
    }

    /**
     * Returns a matcher for a child depth BySelector.
     *
     * @param depth The exact child depth.
     * @return ArgumentMatcher that matches a BySelector for the given depth.
     */
    public static ArgumentMatcher<BySelector> matchesByDepth(final int depth) {
        return new ArgumentMatcher<BySelector>() {
            // Need to do logical matching since BySelector does not override equals(Object).
            @Override
            public boolean matches(BySelector argument) {
                if (argument == null) {
                    return false;
                }
                Integer minDepth = (Integer) getField(argument, "mMinDepth");
                Integer maxDepth = (Integer) getField(argument, "mMaxDepth");
                if (minDepth == null || maxDepth == null) {
                    return false;
                }
                return maxDepth == depth && minDepth == depth;
            }
        };
    }

    /**
     * Returns a matcher for a BySelector that matches a field using a pattern.
     *
     * @param pattern The Pattern to match against.
     * @param fieldName The name of the field that must match the pattern.
     * @return ArgumentMatcher that matches a BySelector for the field using pattern.
     */
    public static ArgumentMatcher<BySelector> matchesByField(Pattern pattern, String fieldName) {
        return new ArgumentMatcher<BySelector>() {
            // Need to do logical matching since BySelector does not override equals(Object).
            @Override
            public boolean matches(BySelector argument) {
                if (argument == null) {
                    return false;
                }
                Pattern p = (Pattern) getField(argument, fieldName);
                if (p == null) {
                    return false;
                }
                return pattern.pattern().equals(p.pattern());
            }
        };
    }

    private static Object getField(Object obj, String fieldName) {
        try {
            Field field = obj.getClass().getDeclaredField(fieldName);
            field.setAccessible(true);
            return field.get(obj);
        } catch (NoSuchFieldException e) {
            System.err.println("Field " + fieldName + " was not found.");
            e.printStackTrace();
            return null;
        } catch (IllegalAccessException e) {
            System.err.println("Field " + fieldName + " was not accessible.");
            e.printStackTrace();
            return null;
        }
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;

import java.util.Locale;

public class TemplateUrlTestHelpers {

    // Starting at a high number so tests can use lower numbers locally.
    private static long sNextNativePtr = 1000;

    /**
     * Same as {@link #buildMockTemplateUrl(String, int, long)}, but uses the construction time for
     * the built object's {@link TemplateUrl#getLastVisitedTime()}.
     */
    public static TemplateUrl buildMockTemplateUrl(String keyword, int prepopulatedId) {
        return buildMockTemplateUrl(keyword, prepopulatedId, System.currentTimeMillis());
    }

    /**
     * Same as {@link #buildMockTemplateUrl(String, int, long, long)}, but selects an arbitrary
     * number, not shared with other objects created through this method, that will be used for the
     * built object's {@link TemplateUrl#getNativePtr}.
     */
    public static TemplateUrl buildMockTemplateUrl(
            String keyword, int prepopulatedId, long lastVisitTime) {
        return buildMockTemplateUrl(keyword, prepopulatedId, lastVisitTime, sNextNativePtr++);
    }

    /**
     * Returns a {@link TemplateUrl} object configured to return the provided arguments when its
     * getters are called.
     */
    public static TemplateUrl buildMockTemplateUrl(
            String keyword, int prepopulatedId, long lastVisitTime, long nativePtr) {
        var templateUrl = mock(TemplateUrl.class);
        lenient().doReturn(keyword).when(templateUrl).getKeyword();
        lenient().doReturn("shortNameFor: " + keyword).when(templateUrl).getShortName();
        lenient().doReturn(prepopulatedId).when(templateUrl).getPrepopulatedId();
        lenient().doReturn(lastVisitTime).when(templateUrl).getLastVisitedTime();
        lenient().doReturn(prepopulatedId != 0).when(templateUrl).getIsPrepopulated();
        lenient().doReturn(nativePtr).when(templateUrl).getNativePtr();
        lenient()
                .doReturn(
                        String.format(
                                Locale.US,
                                "MockTemplateURL -- keyword: %s, prepopulatedId: %d",
                                keyword,
                                prepopulatedId))
                .when(templateUrl)
                .toString();
        return templateUrl;
    }
}

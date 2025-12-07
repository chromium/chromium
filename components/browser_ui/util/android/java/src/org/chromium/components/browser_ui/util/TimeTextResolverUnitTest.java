// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TimeTextResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TimeTextResolverUnitTest {
    private Activity mActivity;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    TimeTextResolver mResolver;

    @Before
    public void setup() {
        mActivity = Robolectric.setupActivity(Activity.class);
    }

    private String fromSecondsAgo(long secondsDelta) {
        return TimeTextResolver.resolveTimeAgoText(
                mActivity.getResources(), TimeUtils.currentTimeMillis() - 1000 * secondsDelta);
    }

    @Test
    public void testLastAccessedTimeAgoText() {
        assertEquals("Today", fromSecondsAgo(60));
        assertEquals("Yesterday", fromSecondsAgo(60 * 60 * 24));
        assertEquals("2 days ago", fromSecondsAgo(60 * 60 * 24 * 2));
        assertEquals("1 week ago", fromSecondsAgo(60 * 60 * 24 * 7));
        assertEquals("2 weeks ago", fromSecondsAgo(60 * 60 * 24 * 7 * 2));
        assertEquals("1 month ago", fromSecondsAgo(60 * 60 * 24 * 31));
        assertEquals("2 months ago", fromSecondsAgo(60 * 60 * 24 * 31 * 2));
    }
}

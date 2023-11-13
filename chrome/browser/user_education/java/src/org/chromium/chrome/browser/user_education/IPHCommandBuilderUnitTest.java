// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the IPHCommandBuilder. */
@RunWith(BaseRobolectricTestRunner.class)
public class IPHCommandBuilderUnitTest {
    private static final String FEATURE_NAME = "Builder Unit Test";
    private static final int CONTENT_STRING_RES = 1234;
    private static final int ACCESSIBILITY_STRING_RES = 4321;
    private IPHCommandBuilder mBuilder;

    @Before
    public void setUp() {
        mBuilder =
                new IPHCommandBuilder(
                        ContextUtils.getApplicationContext().getResources(),
                        FEATURE_NAME,
                        CONTENT_STRING_RES,
                        ACCESSIBILITY_STRING_RES);
    }

    @Test
    public void setInsetRect_nullByDefault() {
        var cmd = mBuilder.build();
        assertNull(cmd.insetRect);
    }

    @Test
    public void setInsetRect_nullInsets() {
        var cmd = mBuilder.setInsetRect(null).build();
        assertNull(cmd.insetRect);
    }

    @Test
    public void setInsetRect_validInsets() {
        var insets = new Rect(10, 20, 30, 40);
        var cmd = mBuilder.setInsetRect(insets).build();
        assertEquals(insets, cmd.insetRect);
    }
}

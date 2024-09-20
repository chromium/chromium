// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests that the content sensitivity of is set properly. The test fixture uses a tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT)
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SensitiveContentTest {
    public static final String SENSITIVE_FILE =
            "/chrome/test/data/autofill/autofill_creditcard_form_with_autocomplete_attributes.html";
    public static final String NOT_SENSITIVE_FILE =
            "/chrome/test/data/autofill/autocomplete_simple_form.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private ContentView mTabContentView;

    @Before
    public void setUp() throws Exception {
        sActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = sActivityTestRule.getTestServer();
        mTabContentView = sActivityTestRule.getActivity().getActivityTab().getContentView();
    }

    @Test
    @MediumTest
    public void testTabHasSensitiveContentWhileSensitiveFieldsArePresent() throws Exception {
        Assert.assertEquals(
                "Initially, the does not have sensitive content",
                mTabContentView.getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
    }
}

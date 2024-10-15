// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
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
    private static class TestSensitiveContentClientObserver
            implements SensitiveContentClient.Observer {
        private boolean mContentIsSensitive;

        @Override
        public void onContentSensitivityChanged(boolean contentIsSensitive) {
            mContentIsSensitive = contentIsSensitive;
        }

        public boolean getContentSensitivity() {
            return mContentIsSensitive;
        }
    }

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
        assertEquals(
                "Initially, the tab does not have sensitive content",
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

    @Test
    @MediumTest
    public void testSensitiveContentClientObserver() throws Exception {
        assertEquals(
                "Initially, the tab does not have sensitive content",
                mTabContentView.getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        final SensitiveContentClient client =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SensitiveContentClient.fromWebContents(
                                        sActivityTestRule.getWebContents()));
        final TestSensitiveContentClientObserver observer =
                new TestSensitiveContentClientObserver();
        ThreadUtils.runOnUiThreadBlocking(() -> client.addObserver(observer));

        assertFalse(observer.getContentSensitivity());
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertTrue(observer.getContentSensitivity());

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        assertFalse(observer.getContentSensitivity());

        // After observation is removed, the observer will not be notified anymore.
        ThreadUtils.runOnUiThreadBlocking(() -> client.removeObserver(observer));
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertFalse(observer.getContentSensitivity());
    }

    @Test
    @MediumTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testTabHasSensitiveContentAttributeIsUpdated() throws Exception {
        assertEquals(
                "Initially, the tab does not have sensitive content",
                mTabContentView.getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        assertFalse(tab.getTabHasSensitiveContent());

        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertTrue(tab.getTabHasSensitiveContent());

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTabContentView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        assertFalse(tab.getTabHasSensitiveContent());
    }
}

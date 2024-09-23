// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.digital_credentials;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.digital_credentials.DigitalIdentityInterstitialClosedReason;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webid.DigitalIdentityProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content.browser.webid.IdentityCredentialsDelegate;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.concurrent.TimeoutException;

/** Integration test for digital identity safety interstitial. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DigitalIdentitySafetyInterstitialIntegrationTest {
    /**
     * Observes shown modal dialogs.
     *
     * <p>Presses {@link ButtonType.POSITIVE} for dialog whose {@link
     * ModalDialogProperties.MESSAGE_PARAGRAPH1} matches the parameter passed to the constructor.
     */
    private static class ModalDialogButtonPresser implements ModalDialogManagerObserver {
        private String mSearchParagraph1;
        private boolean mWasDialogShown;
        private boolean mWasAnyDialogShown;
        private boolean mPressButtonOnShow;
        private PropertyModel mDialogPropertyModel;

        public ModalDialogButtonPresser(String searchParagraph1, boolean pressButtonOnShow) {
            mSearchParagraph1 = searchParagraph1;
            mPressButtonOnShow = pressButtonOnShow;
        }

        private boolean wasAnyDialogShown() {
            return mWasAnyDialogShown;
        }

        public boolean wasDialogShown() {
            return mWasDialogShown;
        }

        public PropertyModel getDialogPropertyModel() {
            return mDialogPropertyModel;
        }

        @Override
        public void onDialogAdded(PropertyModel model) {
            mWasAnyDialogShown = true;

            CharSequence paragraph1 = model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
            if (paragraph1 != null && mSearchParagraph1.equals(paragraph1.toString())) {
                mWasDialogShown = true;
                mDialogPropertyModel = model;
                if (mPressButtonOnShow) {
                    model.get(ModalDialogProperties.CONTROLLER)
                            .onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
                }
            }
        }
    }

    /** {@link IdentityCredentialsDelegate} implementation which returns "token". */
    private static class ReturnTokenIdentityCredentialsDelegate
            extends IdentityCredentialsDelegate {
        @Override
        public Promise<byte[]> get(Activity activity, String origin, String request) {
            return Promise.fulfilled("token".getBytes());
        }
    }

    private static final String TEST_PAGE = "/chrome/test/data/android/fedcm_mdocs.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private EmbeddedTestServer mTestServer;

    private ModalDialogButtonPresser mModalDialogObserver;

    private ModalDialogManager mModalDialogManager;

    public DigitalIdentitySafetyInterstitialIntegrationTest() {}

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mActivityTestRule.getTestServer();
        DigitalIdentityProvider.setDelegateForTesting(new ReturnTokenIdentityCredentialsDelegate());

        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        mModalDialogManager = getActivity().getModalDialogManager();
    }

    @After
    public void tearDown() {
        if (mModalDialogObserver != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mModalDialogManager.removeObserver(mModalDialogObserver);
                    });
        }
    }

    private ChromeTabbedActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    /** Wait till the <textarea> on the test page has the passed-in text content. */
    public void waitTillLogTextAreaHasTextContent(String expectedTextContent)
            throws TimeoutException {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String textContent =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        mActivityTestRule.getWebContents(),
                                        "document.getElementById('log').textContent");
                        Criteria.checkThat(
                                "<textarea> text content is not as expected.",
                                textContent,
                                is(expectedTextContent));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    public void addModalDialogObserver(
            int expectedInterstitialParagraph1ResourceId, boolean pressButtonOnShow) {
        String expectedDialogText = null;
        if (expectedInterstitialParagraph1ResourceId >= 0) {
            expectedDialogText =
                    getActivity()
                            .getString(
                                    expectedInterstitialParagraph1ResourceId,
                                    getPageOriginString());
        }

        mModalDialogObserver = new ModalDialogButtonPresser(expectedDialogText, pressButtonOnShow);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.addObserver(mModalDialogObserver);
                });
    }

    private String getPageOriginString() {
        String pageUrl = mTestServer.getURL(TEST_PAGE);
        Origin pageOrigin = Origin.create(new GURL(pageUrl));
        return pageOrigin.getHost() + ":" + pageOrigin.getPort();
    }

    public void checkDigitalIdentityRequestWithDialogFieldTrialParam(
            String nodeIdToClick, int expectedInterstitialParagraph1ResourceId)
            throws TimeoutException {
        addModalDialogObserver(
                expectedInterstitialParagraph1ResourceId, /* pressButtonOnShow= */ true);

        DOMUtils.clickNode(mActivityTestRule.getWebContents(), nodeIdToClick);

        waitTillLogTextAreaHasTextContent("\"token\"");

        if (expectedInterstitialParagraph1ResourceId >= 0) {
            assertTrue(mModalDialogObserver.wasDialogShown());
        } else {
            assertFalse(mModalDialogObserver.wasAnyDialogShown());
            assertFalse(mModalDialogObserver.wasDialogShown());
        }
    }

    /**
     * Test that when the low risk interstitial feature param is set - The low risk safety
     * interstitial is shown. - The digital identity request succeeds once the user accepts the
     * interstitial.
     */
    @Test
    @LargeTest
    @EnableFeatures("WebIdentityDigitalCredentials:dialog/low_risk")
    public void testShowLowRiskDialog() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                "request_age_only_button",
                R.string.digital_identity_interstitial_low_risk_dialog_text);
    }

    /**
     * Test that when the high risk interstitial feature param is set - The high risk safety
     * interstitial is shown. - The digital identity request succeeds once the user accepts the
     * interstitial.
     */
    @Test
    @LargeTest
    @EnableFeatures("WebIdentityDigitalCredentials:dialog/high_risk")
    public void testShowHighRiskDialog() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                "request_age_only_button",
                R.string.digital_identity_interstitial_high_risk_dialog_text);
    }

    /**
     * Test that the low risk interstitial is shown when credentials other than age are requested.
     */
    @Test
    @LargeTest
    @EnableFeatures("WebIdentityDigitalCredentials:dialog/default")
    public void testShowLowRiskInterstitialWhenRequestCredentialsOtherThanAge()
            throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                "request_age_and_name_button",
                R.string.digital_identity_interstitial_low_risk_dialog_text);
    }

    /** Test that no interstitial is shown by default. */
    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testNoDialogByDefault() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                "request_age_only_button", /* expectedInterstitialParagraph1ResourceId= */ -1);
    }

    /**
     * Test that the feature param takes precedence over the digital credential request type (age or
     * not).
     */
    @Test
    @LargeTest
    @EnableFeatures("WebIdentityDigitalCredentials:dialog/no_dialog")
    public void testFeatureParamTakesPrecedence() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                "request_age_and_name_button", /* expectedInterstitialParagraph1ResourceId= */ -1);
    }

    /**
     * Test that the DigitalIdentityInterstitialClosedReason.PAGE_NAVIGATED is recorded for
     * Blink.DigitalIdentityInterstitialClosedReason if the page navigates while the interstitial is
     * showing.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        "BackForwardCacheMemoryControls",
        "WebIdentityDigitalCredentials:dialog/high_risk"
    })
    public void testCloseReasonUmaRecorded_PageNavigates() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.DigitalIdentityRequest.InterstitialClosedReason",
                        DigitalIdentityInterstitialClosedReason.PAGE_NAVIGATED);
        addModalDialogObserver(
                R.string.digital_identity_interstitial_high_risk_dialog_text,
                /* pressButtonOnShow= */ false);

        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "request_age_and_name_button");
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return mModalDialogObserver.wasDialogShown();
                });

        // Navigating the page should update the interstitial's UI.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/simple.html"));

        histogramWatcher.assertExpected();
    }

    /**
     * Test that Blink.DigitalIdentityInterstitialClosedReason UMA is recorded when the interstitial
     * dialog is closed for a different reason.
     */
    @Test
    @LargeTest
    @EnableFeatures({
        "BackForwardCacheMemoryControls",
        "WebIdentityDigitalCredentials:dialog/high_risk"
    })
    public void testCloseReasonUmaRecorded_Other() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.DigitalIdentityRequest.InterstitialClosedReason",
                        DigitalIdentityInterstitialClosedReason.OK_BUTTON);
        addModalDialogObserver(
                R.string.digital_identity_interstitial_high_risk_dialog_text,
                /* pressButtonOnShow= */ true);

        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "request_age_and_name_button");
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return mModalDialogObserver.wasDialogShown();
                });

        histogramWatcher.assertExpected();
    }
}

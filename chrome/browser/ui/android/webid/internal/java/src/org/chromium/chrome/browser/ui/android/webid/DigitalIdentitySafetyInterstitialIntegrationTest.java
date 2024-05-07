// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

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

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.Promise;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.DigitalCredentialProviderUtils;
import org.chromium.content_public.browser.test.util.DigitalCredentialProviderUtils.MockIdentityCredentialsDelegate;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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

        public ModalDialogButtonPresser(String searchParagraph1) {
            mSearchParagraph1 = searchParagraph1;
        }

        private boolean wasAnyDialogShown() {
            return mWasAnyDialogShown;
        }

        public boolean wasDialogShown() {
            return mWasDialogShown;
        }

        @Override
        public void onDialogAdded(PropertyModel model) {
            mWasAnyDialogShown = true;

            CharSequence paragraph1 = model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
            if (paragraph1 != null && mSearchParagraph1.equals(paragraph1.toString())) {
                mWasDialogShown = true;

                model.get(ModalDialogProperties.CONTROLLER)
                        .onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
            }
        }
    }

    /** {@link MockIdentityCredentialsDelegate} implementation which returns "token". */
    private static class ReturnTokenIdentityCredentialsDelegate
            extends MockIdentityCredentialsDelegate {
        @Override
        public Promise<byte[]> get(Activity activity, String origin, String request) {
            return Promise.fulfilled("token".getBytes());
        }
    }

    /**
     * {@link MockIdentityCredentialsDelegate} implementation which provides the ability to control
     * when the {@link #get()} promise is resolved.
     */
    private static class DelayedReturnIdentityCredentialsDelegate
            extends MockIdentityCredentialsDelegate {
        private Promise<byte[]> mPromise;

        @Override
        public Promise<byte[]> get(Activity activity, String origin, String request) {
            mPromise = new Promise<byte[]>();
            return mPromise;
        }

        public void fulfillPromise() {
            mPromise.fulfill("token".getBytes());
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
        DigitalCredentialProviderUtils.setDelegateForTesting(
                new ReturnTokenIdentityCredentialsDelegate());

        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
    }

    @After
    public void tearDown() {
        if (mModalDialogObserver != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mModalDialogManager.removeObserver(mModalDialogObserver);
                    });
        }
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

    public void setFieldTrialParam(String dialogParamValue) {
        FeatureList.TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS,
                DigitalIdentitySafetyInterstitialBridge.DIGITAL_IDENTITY_DIALOG_PARAM,
                dialogParamValue);
        FeatureList.setTestValues(testValues);
    }

    public void addModalDialogObserver(int expectedInterstitialParagraph1ResourceId) {
        String pageUrl = mTestServer.getURL(TEST_PAGE);
        Origin pageOrigin = Origin.create(new GURL(pageUrl));
        String expectedDialogText = null;
        if (expectedInterstitialParagraph1ResourceId >= 0) {
            expectedDialogText =
                    mActivityTestRule
                            .getActivity()
                            .getString(
                                    expectedInterstitialParagraph1ResourceId,
                                    pageOrigin.getScheme()
                                            + "://"
                                            + pageOrigin.getHost()
                                            + ":"
                                            + pageOrigin.getPort());
        }

        mModalDialogObserver = new ModalDialogButtonPresser(expectedDialogText);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.addObserver(mModalDialogObserver);
                });
    }

    public void checkDigitalIdentityRequestWithDialogFieldTrialParam(
            String dialogParamValue,
            String jsMethodToCall,
            int expectedInterstitialParagraph1ResourceId)
            throws TimeoutException {
        setFieldTrialParam(dialogParamValue);
        addModalDialogObserver(expectedInterstitialParagraph1ResourceId);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), jsMethodToCall);

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
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testShowLowRiskDialog() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                DigitalIdentitySafetyInterstitialBridge
                        .DIGITAL_IDENTITY_LOW_RISK_DIALOG_PARAM_VALUE,
                "requestAgeOnly()",
                R.string.digital_identity_interstitial_low_risk_dialog_text);
    }

    /**
     * Test that when the high risk interstitial feature param is set - The high risk safety
     * interstitial is shown. - The digital identity request succeeds once the user accepts the
     * interstitial.
     */
    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testShowHighRiskDialog() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                DigitalIdentitySafetyInterstitialBridge
                        .DIGITAL_IDENTITY_HIGH_RISK_DIALOG_PARAM_VALUE,
                "requestAgeOnly()",
                R.string.digital_identity_interstitial_high_risk_dialog_text);
    }

    /**
     * Test that the high risk interstitial is shown when credentials other than age are requested.
     */
    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testShowHighRiskInterstitialWhenRequestCredentialsOtherThanAge()
            throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                DigitalIdentitySafetyInterstitialBridge
                        .DIGITAL_IDENTITY_HIGH_RISK_DIALOG_PARAM_VALUE,
                "requestAgeAndName()",
                R.string.digital_identity_interstitial_high_risk_dialog_text);
    }

    /** Test that no interstitial is shown by default. */
    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testNoDialogByDefault() throws TimeoutException {
        checkDigitalIdentityRequestWithDialogFieldTrialParam(
                /* dialogParamValue= */ "",
                "requestAgeOnly()",
                /* expectedInterstitialParagraph1ResourceId= */ -1);
    }

    /**
     * Test that no interstitial is shown if the BF cache is enabled and the page navigates while
     * the Android OS system prompt is being shown.
     */
    @Test
    @LargeTest
    @DisableFeatures({"BackForwardCacheMemoryControls"})
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testNoDialogIfNavigationDuringAndroidOsCall() throws TimeoutException {
        DelayedReturnIdentityCredentialsDelegate delegate =
                new DelayedReturnIdentityCredentialsDelegate();
        DigitalCredentialProviderUtils.setDelegateForTesting(delegate);
        setFieldTrialParam(
                DigitalIdentitySafetyInterstitialBridge
                        .DIGITAL_IDENTITY_HIGH_RISK_DIALOG_PARAM_VALUE);
        addModalDialogObserver(/* expectedInterstitialParagraph1ResourceId= */ -1);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "requestAgeOnly()");

        // Do page navigation during the Android OS call.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/simple.html"));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    delegate.fulfillPromise();
                });

        // An interstitial should not have been shown.
        assertFalse(mModalDialogObserver.wasAnyDialogShown());
        assertFalse(mModalDialogObserver.wasDialogShown());
    }
}

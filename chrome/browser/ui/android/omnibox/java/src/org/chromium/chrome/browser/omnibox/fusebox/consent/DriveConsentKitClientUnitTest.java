// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox.consent;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.TerminationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.DriveDisclaimerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.DriveDisclaimerBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

/** Unit tests for {@link DriveConsentKitClient}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DriveConsentKitClientUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContents;

    @Mock private Profile mProfile;
    @Mock private DriveConsentKitClient.Delegate mDelegate;
    @Mock private DriveDisclaimerBridge.Natives mBridgeNatives;

    private DriveConsentKitClient mClient;

    @Before
    public void setUp() {
        DriveDisclaimerBridgeJni.setInstanceForTesting(mBridgeNatives);
        mClient = new DriveConsentKitClient(mWebContents, mProfile, mDelegate);
    }

    @Test
    public void whenAttached_evaluatesJavaScriptWithCallbackId() {
        mClient.onWhenAttached(42);

        verify(mWebContents).evaluateJavaScript("window.ckUiCallback(42)", /* callback= */ null);
    }

    @Test
    public void closeWithResult_grantedConsent_notifiesDelegate() {
        doReturn(true)
                .when(mBridgeNatives)
                .parseAndSaveConsentResult(mProfile, mWebContents, "payload");

        mClient.onCloseWithResult("{\"encodedPrivacyFlowResult\":\"payload\"}");

        verify(mDelegate).onConsentComplete(/* granted= */ true);
    }

    @Test
    public void closeWithResult_deniedConsent_notifiesDelegate() {
        doReturn(false)
                .when(mBridgeNatives)
                .parseAndSaveConsentResult(mProfile, mWebContents, "payload");

        mClient.onCloseWithResult("{\"encodedPrivacyFlowResult\":\"payload\"}");

        verify(mDelegate).onConsentComplete(/* granted= */ false);
    }

    @Test
    public void closeWithResult_invalidPayload_failsClosed() {
        mClient.onCloseWithResult("invalid-payload");

        verify(mDelegate).onConsentComplete(/* granted= */ false);
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void didFirstVisuallyNonEmptyPaint_notifiesDelegate() {
        mClient.didFirstVisuallyNonEmptyPaint();

        verify(mDelegate).onPageFirstPaint();
    }

    @Test
    public void didFailLoad_primaryMainFrame_notifiesDelegate() {
        mClient.didFailLoad(
                /* isInPrimaryMainFrame= */ true,
                /* errorCode= */ -1,
                GURL.emptyGURL(),
                LifecycleState.ACTIVE);

        verify(mDelegate).onLoadFailed();
    }

    @Test
    public void webContentsDestroyed_failsClosed() {
        mClient.webContentsDestroyed();

        verify(mDelegate).onLoadFailed();
    }

    @Test
    public void terminalOutcome_reportedOnlyOnce() {
        doReturn(true)
                .when(mBridgeNatives)
                .parseAndSaveConsentResult(mProfile, mWebContents, "payload");

        mClient.onCloseWithResult("{\"encodedPrivacyFlowResult\":\"payload\"}");
        mClient.primaryMainFrameRenderProcessGone(TerminationStatus.PROCESS_WAS_KILLED);

        verify(mDelegate).onConsentComplete(/* granted= */ true);
        verify(mDelegate, never()).onLoadFailed();
    }

    @Test
    public void destroy_dropsSubsequentCallbacks() {
        mClient.destroy();

        mClient.primaryMainFrameRenderProcessGone(TerminationStatus.PROCESS_WAS_KILLED);

        verifyNoInteractions(mDelegate);
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinatorSupplier;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Tests for {@link PaymentsWindowCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentsWindowCoordinatorTest {
    private static final String TAB_TITLE = "Issuer Name";
    private static final GURL ISSUER_URL = new GURL("https://www.example.com/");

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private WebContents mMerchantWebContents;
    @Mock private EphemeralTabCoordinator mEphemeralTabCoordinator;
    @Mock private EphemeralTabObserver mEphemeralTabObserver;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile.Natives mProfileNatives;
    @Mock private PaymentsWindowBridge mPaymentsWindowBridge;
    @Captor private ArgumentCaptor<EphemeralTabObserver> mEphemeralTabObserverCaptor;

    private PaymentsWindowCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator = new PaymentsWindowCoordinator(mPaymentsWindowBridge);
    }

    @After
    public void tearDown() {
        EphemeralTabCoordinatorSupplier.setInstanceForTesting(null);
        ProfileJni.setInstanceForTesting(null);
    }

    private void setUpForEphemeralTabObserverTest() {
        when(mMerchantWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        EphemeralTabCoordinatorSupplier.setInstanceForTesting(mEphemeralTabCoordinator);
        ProfileJni.setInstanceForTesting(mProfileNatives);
        when(mProfileNatives.fromWebContents(eq(mMerchantWebContents))).thenReturn(mProfile);
        mCoordinator.openEphemeralTab(ISSUER_URL, TAB_TITLE, mMerchantWebContents);
    }

    @Test
    public void testOpenEphemeralTab_whenSuccess_thenRequestsToOpenSheet() {
        when(mMerchantWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        EphemeralTabCoordinatorSupplier.setInstanceForTesting(mEphemeralTabCoordinator);
        doNothing().when(mEphemeralTabCoordinator).addObserver(mEphemeralTabObserver);
        ProfileJni.setInstanceForTesting(mProfileNatives);
        when(mProfileNatives.fromWebContents(eq(mMerchantWebContents))).thenReturn(mProfile);

        mCoordinator.openEphemeralTab(ISSUER_URL, TAB_TITLE, mMerchantWebContents);

        verify(mEphemeralTabCoordinator)
                .requestOpenSheet(
                        /* url= */ ISSUER_URL,
                        /* fullPageUrl= */ null,
                        /* title= */ TAB_TITLE,
                        mProfile,
                        /* canPromoteToNewTab= */ false);
        verify(mEphemeralTabCoordinator).addObserver(any(EphemeralTabObserver.class));
    }

    @Test
    public void testOpenEphemeralTab_whenInvalidWindowAndroid_thenDoesNotRequestToOpenSheet() {
        when(mMerchantWebContents.getTopLevelNativeWindow()).thenReturn(/* windowAndroid= */ null);

        mCoordinator.openEphemeralTab(ISSUER_URL, TAB_TITLE, mMerchantWebContents);

        verify(mEphemeralTabCoordinator, never())
                .requestOpenSheet(
                        /* url= */ ISSUER_URL,
                        /* fullPageUrl= */ null,
                        /* title= */ TAB_TITLE,
                        mProfile,
                        /* canPromoteToNewTab= */ false);
        verify(mEphemeralTabCoordinator, never()).addObserver(any(EphemeralTabObserver.class));
    }

    @Test
    public void testOpenEphemeralTab_whenInvalidCoordinator_thenDoesNotRequestToOpenSheet() {
        when(mMerchantWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);

        mCoordinator.openEphemeralTab(ISSUER_URL, TAB_TITLE, mMerchantWebContents);

        verify(mEphemeralTabCoordinator, never())
                .requestOpenSheet(
                        /* url= */ ISSUER_URL,
                        /* fullPageUrl= */ null,
                        /* title= */ TAB_TITLE,
                        mProfile,
                        /* canPromoteToNewTab= */ false);
        verify(mEphemeralTabCoordinator, never()).addObserver(any(EphemeralTabObserver.class));
    }

    @Test
    public void testCloseEphemeralTab_whenOpen_thenClosesTab() {
        mCoordinator.setEphemeralTabCoordinatorForTesting(mEphemeralTabCoordinator);
        doNothing().when(mEphemeralTabCoordinator).close();
        doReturn(true).when(mEphemeralTabCoordinator).isOpened();

        mCoordinator.closeEphemeralTab();

        verify(mEphemeralTabCoordinator).close();
    }

    @Test
    public void testCloseEphemeralTab_whenNotOpen_thenDoesNotcloseTab() {
        mCoordinator.setEphemeralTabCoordinatorForTesting(mEphemeralTabCoordinator);
        doReturn(false).when(mEphemeralTabCoordinator).isOpened();

        mCoordinator.closeEphemeralTab();

        verify(mEphemeralTabCoordinator, never()).close();
    }

    @Test
    public void testEphemeralTabObserver_OnNavigationFinished_ForwardsCallToBridge() {
        setUpForEphemeralTabObserverTest();
        verify(mEphemeralTabCoordinator).addObserver(mEphemeralTabObserverCaptor.capture());

        mEphemeralTabObserverCaptor.getValue().onNavigationFinished(ISSUER_URL);

        verify(mPaymentsWindowBridge).onNavigationFinished(ISSUER_URL);
    }

    @Test
    public void testEphemeralTabObserver_OnWebContentsObservationStarted_ForwardsCallToBridge() {
        setUpForEphemeralTabObserverTest();
        verify(mEphemeralTabCoordinator).addObserver(mEphemeralTabObserverCaptor.capture());

        mEphemeralTabObserverCaptor
                .getValue()
                .onWebContentsObservationStarted(mMerchantWebContents);

        verify(mPaymentsWindowBridge).onWebContentsObservationStarted(mMerchantWebContents);
    }

    @Test
    public void testEphemeralTabObserver_OnWebContentsDestroyed_ForwardsCallToBridge() {
        setUpForEphemeralTabObserverTest();
        verify(mEphemeralTabCoordinator).addObserver(mEphemeralTabObserverCaptor.capture());
        EphemeralTabObserver ephemeralTabObserver = mEphemeralTabObserverCaptor.getValue();

        ephemeralTabObserver.onWebContentsDestroyed();

        verify(mEphemeralTabCoordinator).removeObserver(ephemeralTabObserver);
        verify(mPaymentsWindowBridge).onWebContentsDestroyed();
    }
}

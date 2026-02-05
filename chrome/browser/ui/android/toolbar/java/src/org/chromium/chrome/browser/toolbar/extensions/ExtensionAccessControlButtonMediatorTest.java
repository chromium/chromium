// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for ExtensionAccessControlButtonMediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionAccessControlButtonMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private View mRequestAccessButton;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();

    @Captor private ArgumentCaptor<ExtensionsToolbarBridge.Observer> mToolbarObserverCaptor;

    private ExtensionAccessControlButtonMediator mMediator;

    @Before
    public void setUp() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mCurrentTabSupplier.set(mTab);

        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(new RequestAccessButtonParams(new String[0], ""));

        mMediator =
                new ExtensionAccessControlButtonMediator(
                        mCurrentTabSupplier, mExtensionsToolbarBridge, mRequestAccessButton);
        verify(mExtensionsToolbarBridge).addObserver(mToolbarObserverCaptor.capture());

        clearInvocations(mRequestAccessButton);
    }

    @Test
    // Tests that the requests access button is visible only when there are extensions requesting
    // access
    public void testRequestAccessButtonVisibility() {
        ExtensionsToolbarBridge.Observer observer = mToolbarObserverCaptor.getValue();

        // No requests.
        RequestAccessButtonParams paramsNoRequests =
                new RequestAccessButtonParams(new String[0], "");
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsNoRequests);

        observer.onRequestAccessButtonParamsChanged();
        verify(mRequestAccessButton).setVisibility(View.GONE);

        // Has requests.
        String tooltip = "Some tooltip";
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsWithRequests);

        observer.onRequestAccessButtonParamsChanged();
        verify(mRequestAccessButton).setVisibility(View.VISIBLE);
        verify(mRequestAccessButton).setContentDescription(tooltip);
    }
}

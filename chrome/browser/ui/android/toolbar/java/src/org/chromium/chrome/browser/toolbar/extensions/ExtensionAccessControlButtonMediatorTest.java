// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for ExtensionAccessControlButtonMediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionAccessControlButtonMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();

    @Captor private ArgumentCaptor<ExtensionsToolbarBridge.Observer> mToolbarObserverCaptor;

    private ExtensionAccessControlButtonMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mCurrentTabSupplier.set(mTab);

        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(new RequestAccessButtonParams(new String[0], ""));

        mModel =
                new PropertyModel.Builder(
                                PropertyModel.concatKeys(
                                        ExtensionsMenuProperties.ALL_KEYS,
                                        ExtensionsToolbarProperties.ALL_KEYS))
                        .build();

        mMediator =
                new ExtensionAccessControlButtonMediator(
                        mModel, mCurrentTabSupplier, mExtensionsToolbarBridge, (v) -> {});
        verify(mExtensionsToolbarBridge).addObserver(mToolbarObserverCaptor.capture());
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
        assertFalse(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));

        // Has requests.
        String tooltip = "Some tooltip";
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsWithRequests);

        observer.onRequestAccessButtonParamsChanged();
        assertTrue(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));
        assertEquals(
                tooltip,
                mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TEXT));
    }

    @Test
    public void testRequestAccessButtonVisibility_withWebContents() {
        ExtensionsToolbarBridge.Observer observer = mToolbarObserverCaptor.getValue();

        // No requests.
        RequestAccessButtonParams paramsNoRequests =
                new RequestAccessButtonParams(new String[0], "");
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsNoRequests);

        observer.onActiveWebContentsChanged(mWebContents);
        assertFalse(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));

        // Has requests.
        String tooltip = "Some tooltip";
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsWithRequests);

        observer.onActiveWebContentsChanged(mWebContents);
        assertTrue(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));
        assertEquals(
                tooltip,
                mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION));
        assertEquals(1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TEXT));
    }
}

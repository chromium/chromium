// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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
    @Mock private Context mContext;
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();

    @Captor private ArgumentCaptor<ExtensionsToolbarBridge.Observer> mToolbarObserverCaptor;

    private ExtensionAccessControlButtonMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mCurrentTabSupplier.set(mTab);
        when(mContext.getString(
                        org.chromium.chrome.browser.ui.extensions.R.string
                                .extensions_request_access_button_dismissed_text))
                .thenReturn("Allowed");

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
                        mContext, mModel, mCurrentTabSupplier, mExtensionsToolbarBridge, (v) -> {});
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
        assertEquals(
                1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT));
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
        assertEquals(
                1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT));
    }

    @Test
    public void testRequestAccessButton_TabSwitchClearsAllowedState() {
        ExtensionsToolbarBridge.Observer observer = mToolbarObserverCaptor.getValue();

        // 1. Initial state: Has requests on Tab A
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, "Tooltip");
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(mWebContents))
                .thenReturn(paramsWithRequests);

        observer.onActiveWebContentsChanged(mWebContents);
        assertTrue(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));
        assertEquals(
                1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT));

        // 2. User clicks the button
        android.view.View.OnClickListener listener =
                mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CLICK_LISTENER);
        listener.onClick(null);

        // 3. Button changes to "Allowed" state (-1)
        assertEquals(
                -1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT));

        // 4. Same tab reload (WebContents doesn't change) should NOT clear the "Allowed" text
        observer.onActiveWebContentsChanged(mWebContents);
        assertEquals(
                -1, mModel.get(ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_EXTENSION_COUNT));

        // 5. Switch to a new tab (different WebContents)
        WebContents newWebContents = mock(WebContents.class);
        RequestAccessButtonParams paramsNewTab = new RequestAccessButtonParams(new String[0], "");
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(newWebContents))
                .thenReturn(paramsNewTab);

        observer.onActiveWebContentsChanged(newWebContents);

        // State should be immediately cleared and the button should evaluate state for the new tab
        // (no requests)
        assertFalse(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));
    }
}

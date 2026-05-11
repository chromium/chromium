// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.Display;
import android.view.View.OnClickListener;
import android.view.WindowManager;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.RequestAccessButtonParams;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for ExtensionAccessControlButtonMediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionAccessControlButtonMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private Context mContext;
    @Mock private Supplier<Boolean> mIsWindowCompactSupplier;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ManagedMessageDispatcher mMessageDispatcher;

    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();

    @Captor private ArgumentCaptor<ExtensionsToolbarBridge.Observer> mToolbarObserverCaptor;

    private ExtensionAccessControlButtonMediator mMediator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mCurrentTabSupplier.set(mTab);
        when(mIsWindowCompactSupplier.get()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));
        DisplayAndroid displayAndroid = mock(DisplayAndroid.class);
        when(displayAndroid.getDipScale()).thenReturn(1.0f);
        when(mWindowAndroid.getDisplay()).thenReturn(displayAndroid);
        DisplayAndroid.setNonMultiDisplayForTesting(displayAndroid);

        Resources resources = mock(Resources.class);
        when(mContext.getResources()).thenReturn(resources);
        when(resources.getDimensionPixelSize(anyInt())).thenReturn(10);
        WindowManager windowManager = mock(WindowManager.class);
        when(mContext.getSystemService(Context.WINDOW_SERVICE)).thenReturn(windowManager);
        Display display = mock(Display.class);
        when(windowManager.getDefaultDisplay()).thenReturn(display);

        MessagesFactory.attachMessageDispatcher(mWindowAndroid, mMessageDispatcher);

        when(mContext.getString(R.string.extensions_request_access_button_dismissed_text))
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
                        mContext,
                        mModel,
                        mCurrentTabSupplier,
                        mExtensionsToolbarBridge,
                        (v) -> {},
                        mIsWindowCompactSupplier);
        verify(mExtensionsToolbarBridge).addObserver(mToolbarObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
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
        OnClickListener listener =
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

    @Test
    public void testRequestAccessButtonVisibility_CompactWindow() {
        ExtensionsToolbarBridge.Observer observer = mToolbarObserverCaptor.getValue();
        when(mIsWindowCompactSupplier.get()).thenReturn(true);

        String tooltip = "Some tooltip";
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsWithRequests);
        when(mContext.getString(
                        R.string.extensions_request_access_message_title_single_extension,
                        "ExtensionName"))
                .thenReturn("Allow extension \"ExtensionName\"?");
        when(mContext.getString(R.string.extensions_request_access_button))
                .thenReturn("Allow $1 extensions?");
        when(mContext.getString(R.string.extensions_menu_requests_access_section_allow_button_text))
                .thenReturn("Allow");

        ExtensionAction action = mock(ExtensionAction.class);
        when(action.getName()).thenReturn("ExtensionName");
        when(mExtensionsToolbarBridge.getAction("a", mWebContents)).thenReturn(action);

        observer.onRequestAccessButtonParamsChanged();

        // Button should be hidden when window is compact
        assertFalse(mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE));

        // But message should be enqueued
        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueWindowScopedMessage(messageCaptor.capture(), any(Boolean.class));

        PropertyModel messageModel = messageCaptor.getValue();
        assertEquals(
                "Allow extension \"ExtensionName\"?",
                messageModel.get(MessageBannerProperties.TITLE));

        // Test primary action click behavior
        Supplier<Integer> primaryAction =
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION);
        int result = primaryAction.get();
        assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, result);
        verify(mExtensionsToolbarBridge).onRequestAccessButtonClicked(mWebContents);

        // Test multi-extension case
        RequestAccessButtonParams multiParams =
                new RequestAccessButtonParams(new String[] {"a", "b"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any())).thenReturn(multiParams);

        // Dismiss first message so a new one can be enqueued
        Callback<Integer> dismissAction = messageModel.get(MessageBannerProperties.ON_DISMISSED);
        dismissAction.onResult(DismissReason.UNKNOWN);

        // Advance time to allow the "Allowed" text to clear.
        // The mediator hides the button while showing the "Allowed" text for 4 seconds.
        // We must advance the looper to simulate this delay passing before we can
        // show the next message.
        ShadowLooper.idleMainLooper(4000, TimeUnit.MILLISECONDS);

        observer.onRequestAccessButtonParamsChanged();

        verify(mMessageDispatcher, times(2))
                .enqueueWindowScopedMessage(messageCaptor.capture(), any(Boolean.class));
        PropertyModel multiMessageModel = messageCaptor.getValue();
        assertEquals("Allow 2 extensions?", multiMessageModel.get(MessageBannerProperties.TITLE));
    }

    @Test
    public void testRequestAccessButtonVisibility_CompactWindow_HighDipScale() {
        DisplayAndroid displayAndroid = mock(DisplayAndroid.class);
        when(displayAndroid.getDipScale()).thenReturn(2.0f);
        when(mWindowAndroid.getDisplay()).thenReturn(displayAndroid);

        ExtensionsToolbarBridge.Observer observer = mToolbarObserverCaptor.getValue();
        when(mIsWindowCompactSupplier.get()).thenReturn(true);

        String tooltip = "Some tooltip";
        RequestAccessButtonParams paramsWithRequests =
                new RequestAccessButtonParams(new String[] {"a"}, tooltip);
        when(mExtensionsToolbarBridge.getRequestAccessButtonParams(any()))
                .thenReturn(paramsWithRequests);

        ExtensionAction action = mock(ExtensionAction.class);
        when(action.getName()).thenReturn("ExtensionName");
        when(mExtensionsToolbarBridge.getAction("a", mWebContents)).thenReturn(action);

        observer.onRequestAccessButtonParamsChanged();

        // Verify that getIcon was called with scaled dimensions.
        // widthDp = 10 / 2 = 5
        // heightDp = 10 / 2 = 5
        // scaleFactor = 2.0f
        verify(mExtensionsToolbarBridge).getIcon("a", mWebContents, 5, 5, 2.0f);
    }
}

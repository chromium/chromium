// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** A test for {@link ScopeChangeController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScopeChangeControllerTest {
    private static final boolean IS_SAME_DOCUMENT = true;
    private static final boolean IS_RELOAD = true;
    private static final boolean DID_COMMIT = true;

    @Test
    @SmallTest
    public void testNavigationScopeChange() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);

        int expectedOnScopeChangeCalls = 0;
        ScopeKey key = new ScopeKey(MessageScopeType.NAVIGATION, webContents);
        controller.firstMessageEnqueued(key);

        final ArgumentCaptor<WebContentsObserver> runnableCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(webContents).addObserver(runnableCaptor.capture());

        WebContentsObserver observer = runnableCaptor.getValue();

        // Default visibility of web contents is invisible.
        expectedOnScopeChangeCalls++;
        ArgumentCaptor<MessageScopeChange> captor =
                ArgumentCaptor.forClass(MessageScopeChange.class);
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE,
                captor.getValue().changeType);

        observer.onVisibilityChanged(Visibility.VISIBLE);
        expectedOnScopeChangeCalls++;

        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is shown"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be active when page is shown",
                ChangeType.ACTIVE,
                captor.getValue().changeType);

        observer.onVisibilityChanged(Visibility.HIDDEN);
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE,
                captor.getValue().changeType);

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandle(!IS_SAME_DOCUMENT, IS_RELOAD, DID_COMMIT));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should not be called for a refresh"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandle(!IS_SAME_DOCUMENT, !IS_RELOAD, !DID_COMMIT));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should not be called for uncommitted"
                                                + " navigations"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandle(IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should not be called for same document"
                                                + " navigations"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandle(!IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should be called when page is navigated to"
                                                + " another page"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be destroy when navigated to another page",
                ChangeType.DESTROY,
                captor.getValue().changeType);

        observer.onTopLevelNativeWindowChanged(null);
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should be called when top level native window"
                                                + " changes"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be destroy when top level native window changes",
                ChangeType.DESTROY,
                captor.getValue().changeType);
    }

    @Test
    @SmallTest
    public void testIgnoreNavigation() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);
        ScopeKey key = new ScopeKey(MessageScopeType.WEB_CONTENTS, webContents);
        controller.firstMessageEnqueued(key);

        final ArgumentCaptor<WebContentsObserver> runnableCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(webContents).addObserver(runnableCaptor.capture());

        WebContentsObserver observer = runnableCaptor.getValue();

        // Default visibility of web contents is invisible.
        ArgumentCaptor<MessageScopeChange> captor =
                ArgumentCaptor.forClass(MessageScopeChange.class);
        verify(delegate, description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE,
                captor.getValue().changeType);

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandle(!IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT));
        verify(
                        delegate,
                        times(1).description(
                                        "Delegate should not be called when navigation is ignored"))
                .onScopeChange(any());
    }

    @Test
    @SmallTest
    public void testOriginScopeChange() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);

        int expectedOnScopeChangeCalls = 0;
        ScopeKey key = new ScopeKey(MessageScopeType.ORIGIN, webContents);
        controller.firstMessageEnqueued(key);
        final GURL gurl1 = JUnitTestGURLs.GOOGLE_URL;
        final GURL gurl2 = JUnitTestGURLs.GOOGLE_URL_DOG;
        final GURL gurl3 = JUnitTestGURLs.EXAMPLE_URL;

        final ArgumentCaptor<WebContentsObserver> runnableCaptor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify(webContents).addObserver(runnableCaptor.capture());

        WebContentsObserver observer = runnableCaptor.getValue();

        // Default visibility of web contents is invisible.
        expectedOnScopeChangeCalls++;
        ArgumentCaptor<MessageScopeChange> captor =
                ArgumentCaptor.forClass(MessageScopeChange.class);
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE,
                captor.getValue().changeType);

        observer.onVisibilityChanged(Visibility.VISIBLE);
        expectedOnScopeChangeCalls++;

        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is shown"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be active when page is shown",
                ChangeType.ACTIVE,
                captor.getValue().changeType);

        observer.onVisibilityChanged(Visibility.HIDDEN);
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE,
                captor.getValue().changeType);

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandleWithUrl(!IS_SAME_DOCUMENT, IS_RELOAD, DID_COMMIT, gurl1));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description("Delegate should not be called for a refresh"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandleWithUrl(!IS_SAME_DOCUMENT, !IS_RELOAD, !DID_COMMIT, gurl1));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should not be called for uncommitted"
                                                + " navigations"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandleWithUrl(IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT, gurl1));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should not be called for same document"
                                                + " navigations"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandleWithUrl(!IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT, gurl2));
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should be not called when page is navigated to"
                                                + " same domain"))
                .onScopeChange(any());

        observer.didFinishNavigationInPrimaryMainFrame(
                createNavigationHandleWithUrl(!IS_SAME_DOCUMENT, !IS_RELOAD, DID_COMMIT, gurl3));
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should be called when page is navigated to"
                                                + " another domain"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be destroy when navigated to another domain",
                ChangeType.DESTROY,
                captor.getValue().changeType);

        observer.onTopLevelNativeWindowChanged(null);
        expectedOnScopeChangeCalls++;
        verify(
                        delegate,
                        times(expectedOnScopeChangeCalls)
                                .description(
                                        "Delegate should be called when top level native window"
                                                + " changes"))
                .onScopeChange(captor.capture());
        Assert.assertEquals(
                "Scope type should be destroy when top level native window changes",
                ChangeType.DESTROY,
                captor.getValue().changeType);
    }

    private NavigationHandle createNavigationHandle(
            boolean isSameDocument, boolean isReload, boolean didCommit) {
        return createNavigationHandleWithUrl(isSameDocument, isReload, didCommit, null);
    }

    private NavigationHandle createNavigationHandleWithUrl(
            boolean isSameDocument, boolean isReload, boolean didCommit, GURL url) {
        NavigationHandle handle =
                NavigationHandle.createForTesting(
                        url,
                        /* isInPrimaryMainFrame= */ true,
                        isSameDocument,
                        /* isRendererInitiated= */ true,
                        /* pageTransition= */ 0,
                        /* hasUserGesture= */ false,
                        isReload);
        handle.didFinish(
                url,
                /* isErrorPage= */ false,
                didCommit,
                /* isFragmentNavigation= */ false,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                /* transition */ 0,
                /* errorCode= */ 0,
                /* httpStatusCode= */ 0,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                /* isSaveableNavigation= */ false);
        return handle;
    }
}

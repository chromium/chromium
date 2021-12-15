// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.PageTransition;

/**
 * A test for {@link ScopeChangeController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ScopeChangeControllerTest {
    @Test
    @SmallTest
    public void testNavigationScopeChange() {
        ScopeChangeController.Delegate delegate =
                Mockito.mock(ScopeChangeController.Delegate.class);
        ScopeChangeController controller = new ScopeChangeController(delegate);

        MockWebContents webContents = mock(MockWebContents.class);
        NavigationController navigationController = mock(NavigationController.class);
        NavigationEntry entry = mock(NavigationEntry.class);
        when(webContents.getNavigationController()).thenReturn(navigationController);
        when(navigationController.getLastCommittedEntryIndex()).thenReturn(1);
        when(navigationController.getEntryAtIndex(anyInt())).thenReturn(entry);
        when(entry.getTransition()).thenReturn(PageTransition.HOME_PAGE);

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
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.onWebContentsFocused();
        expectedOnScopeChangeCalls++;

        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is shown"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be active when page is shown", ChangeType.ACTIVE,
                captor.getValue().changeType);

        observer.onWebContentsLostFocus();
        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should be called when page is hidden"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.navigationEntryCommitted(createLoadCommittedDetails(true));
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description("Delegate should not be called when entry is replaced"))
                .onScopeChange(any());

        observer.navigationEntryCommitted(createLoadCommittedDetails(false));

        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description(
                                "Delegate should be called when page is navigated to another page"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be destroy when navigated to another page",
                ChangeType.DESTROY, captor.getValue().changeType);

        observer.onTopLevelNativeWindowChanged(null);

        expectedOnScopeChangeCalls++;
        verify(delegate,
                times(expectedOnScopeChangeCalls)
                        .description(
                                "Delegate should be called when top level native window changes"))
                .onScopeChange(captor.capture());
        Assert.assertEquals("Scope type should be destroy when top level native window changes",
                ChangeType.DESTROY, captor.getValue().changeType);
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
        Assert.assertEquals("Scope type should be inactive when page is hidden",
                ChangeType.INACTIVE, captor.getValue().changeType);

        observer.navigationEntryCommitted(createLoadCommittedDetails(false));
        verify(delegate,
                times(1).description("Delegate should not be called when navigation is ignored"))
                .onScopeChange(any());
    }

    private LoadCommittedDetails createLoadCommittedDetails(boolean didReplaceEntry) {
        return new LoadCommittedDetails(-1, null, didReplaceEntry, false, true, -1);
    }
}

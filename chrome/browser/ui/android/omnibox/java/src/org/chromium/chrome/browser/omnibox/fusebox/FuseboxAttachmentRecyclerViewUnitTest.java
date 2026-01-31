// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerView.ScrollToEndOnInsertionObserver;

/** Unit tests for {@link FuseboxAttachmentRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentRecyclerViewUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock FuseboxAttachmentRecyclerView mScrollToEndOnInsertionMockView;
    private @Mock RecyclerView.Adapter mAdapter;

    private ScrollToEndOnInsertionObserver mScrollToEndOnInsertionObserver;

    @Before
    public void setUp() {
        lenient().when(mScrollToEndOnInsertionMockView.getAdapter()).thenReturn(mAdapter);
        mScrollToEndOnInsertionObserver =
                new ScrollToEndOnInsertionObserver(mScrollToEndOnInsertionMockView);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEnd() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(0, 1);
        verify(mScrollToEndOnInsertionMockView).scrollToPosition(0);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEndWithMultipleItems() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(10, 5);
        verify(mScrollToEndOnInsertionMockView).scrollToPosition(14);
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnRemove() {
        mScrollToEndOnInsertionObserver.onItemRangeRemoved(0, 1);
        verify(mScrollToEndOnInsertionMockView, never()).scrollToPosition(anyInt());
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnMove() {
        mScrollToEndOnInsertionObserver.onItemRangeMoved(0, 1, 1);
        verify(mScrollToEndOnInsertionMockView, never()).scrollToPosition(anyInt());
    }
}

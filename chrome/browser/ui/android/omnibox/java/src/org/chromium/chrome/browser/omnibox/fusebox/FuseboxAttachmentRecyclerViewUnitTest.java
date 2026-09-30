// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerView.ScrollToEndOnInsertionObserver;

/** Unit tests for {@link FuseboxAttachmentRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentRecyclerViewUnitTest {
    private static class TestRecyclerView extends RecyclerView {
        private int mScrollToPosition = RecyclerView.NO_POSITION;
        private int mScrollCallCount;

        TestRecyclerView(Context context) {
            super(context);
        }

        @Override
        public void scrollToPosition(int position) {
            mScrollToPosition = position;
            mScrollCallCount++;
        }
    }

    private TestRecyclerView mRecyclerView;
    private ScrollToEndOnInsertionObserver mScrollToEndOnInsertionObserver;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mRecyclerView = new TestRecyclerView(context);
        mScrollToEndOnInsertionObserver = new ScrollToEndOnInsertionObserver(mRecyclerView);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEnd() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(0, 1);
        assertEquals(0, mRecyclerView.mScrollToPosition);
        assertEquals(1, mRecyclerView.mScrollCallCount);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEndWithMultipleItems() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(10, 5);
        assertEquals(14, mRecyclerView.mScrollToPosition);
        assertEquals(1, mRecyclerView.mScrollCallCount);
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnRemove() {
        mScrollToEndOnInsertionObserver.onItemRangeRemoved(0, 1);
        assertEquals(RecyclerView.NO_POSITION, mRecyclerView.mScrollToPosition);
        assertEquals(0, mRecyclerView.mScrollCallCount);
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnMove() {
        mScrollToEndOnInsertionObserver.onItemRangeMoved(0, 1, 1);
        assertEquals(RecyclerView.NO_POSITION, mRecyclerView.mScrollToPosition);
        assertEquals(0, mRecyclerView.mScrollCallCount);
    }
}

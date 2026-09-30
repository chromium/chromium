// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerView.ScrollToEndOnInsertionObserver;

/** Unit tests for {@link FuseboxAttachmentRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentRecyclerViewUnitTest {
    private static class TestFuseboxAttachmentRecyclerView extends FuseboxAttachmentRecyclerView {
        int mLastScrolledPosition = RecyclerView.NO_POSITION;

        TestFuseboxAttachmentRecyclerView(Context context) {
            super(context, null);
        }

        @Override
        public void scrollToPosition(int position) {
            mLastScrolledPosition = position;
        }
    }

    private TestFuseboxAttachmentRecyclerView mView;
    private ScrollToEndOnInsertionObserver mScrollToEndOnInsertionObserver;

    @Before
    public void setUp() {
        mView = new TestFuseboxAttachmentRecyclerView(ContextUtils.getApplicationContext());
        mScrollToEndOnInsertionObserver = new ScrollToEndOnInsertionObserver(mView);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEnd() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(0, 1);
        assertEquals(0, mView.mLastScrolledPosition);
    }

    @Test
    public void scrollToEndOnInsertionObserver_scrollsToEndWithMultipleItems() {
        mScrollToEndOnInsertionObserver.onItemRangeInserted(10, 5);
        assertEquals(14, mView.mLastScrolledPosition);
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnRemove() {
        mScrollToEndOnInsertionObserver.onItemRangeRemoved(0, 1);
        assertEquals(RecyclerView.NO_POSITION, mView.mLastScrolledPosition);
    }

    @Test
    public void scrollToEndOnInsertionObserver_doesNotScrollOnMove() {
        mScrollToEndOnInsertionObserver.onItemRangeMoved(0, 1, 1);
        assertEquals(RecyclerView.NO_POSITION, mView.mLastScrolledPosition);
    }
}

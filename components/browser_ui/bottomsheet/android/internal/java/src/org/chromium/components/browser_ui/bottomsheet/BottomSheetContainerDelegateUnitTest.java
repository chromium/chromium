// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BottomSheetContainerDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetContainerDelegateUnitTest {
    private Context mContext;
    private FrameLayout mDecorParent;
    private FrameLayout mContentRoot;
    private FrameLayout mOuterRoot;
    private FrameLayout mSheetContainer;
    private BottomSheetContainerDelegate mDelegate;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mDecorParent = new FrameLayout(mContext);
        mContentRoot = new FrameLayout(mContext);
        mContentRoot.setId(android.R.id.content);
        mOuterRoot = new FrameLayout(mContext);
        mSheetContainer = new FrameLayout(mContext);
        mSheetContainer.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mOuterRoot.addView(mSheetContainer);
        mContentRoot.addView(mOuterRoot);
        mDecorParent.addView(mContentRoot);
        mDelegate = new BottomSheetContainerDelegate(mSheetContainer);
    }

    @Test
    public void testSetContainerBottomMargin() {
        mDelegate.setContainerBottomMargin(48);
        assertEquals(48, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);

        mDelegate.setContainerBottomMargin(0);
        assertEquals(0, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
    }

    @Test
    public void testSetContainerClipToPadding() {
        mSheetContainer.setClipChildren(true);
        mSheetContainer.setClipToPadding(true);

        mDelegate.setContainerClipToPadding(false);
        assertFalse(mSheetContainer.getClipChildren());
        assertFalse(mSheetContainer.getClipToPadding());

        mDelegate.setContainerClipToPadding(true);
        assertFalse(mSheetContainer.getClipChildren());
        assertTrue(mSheetContainer.getClipToPadding());
    }

    @Test
    public void testSetAncestorsUnclipped_StopsAtContentRootAndRestoresSavedState() {
        mDecorParent.setClipChildren(true);
        mDecorParent.setClipToPadding(true);
        mContentRoot.setClipChildren(true);
        mContentRoot.setClipToPadding(false);
        mOuterRoot.setClipChildren(true);
        mOuterRoot.setClipToPadding(true);

        mDelegate.setAncestorsUnclipped(true);
        assertFalse(mOuterRoot.getClipChildren());
        assertFalse(mOuterRoot.getClipToPadding());
        assertFalse(mContentRoot.getClipChildren());
        assertFalse(mContentRoot.getClipToPadding());
        assertTrue(mDecorParent.getClipChildren());
        assertTrue(mDecorParent.getClipToPadding());

        // Repeated true is idempotent and preserves the originally captured state.
        mDelegate.setAncestorsUnclipped(true);
        mDelegate.setAncestorsUnclipped(false);
        assertTrue(mOuterRoot.getClipChildren());
        assertTrue(mOuterRoot.getClipToPadding());
        assertTrue(mContentRoot.getClipChildren());
        assertFalse(mContentRoot.getClipToPadding());
        assertTrue(mDecorParent.getClipChildren());
        assertTrue(mDecorParent.getClipToPadding());

        // Repeated false is a no-op and does not overwrite external ancestor clip flags.
        mOuterRoot.setClipChildren(false);
        mDelegate.setAncestorsUnclipped(false);
        assertFalse(mOuterRoot.getClipChildren());
    }

    @Test
    public void testSetAncestorsUnclipped_PreservesAlreadyUnclippedAncestors() {
        mOuterRoot.setClipChildren(false);
        mOuterRoot.setClipToPadding(false);
        mContentRoot.setClipChildren(true);
        mContentRoot.setClipToPadding(true);

        mDelegate.setAncestorsUnclipped(true);
        assertFalse(mOuterRoot.getClipChildren());
        assertFalse(mOuterRoot.getClipToPadding());
        assertFalse(mContentRoot.getClipChildren());
        assertFalse(mContentRoot.getClipToPadding());

        mDelegate.setAncestorsUnclipped(false);
        assertFalse(mOuterRoot.getClipChildren());
        assertFalse(mOuterRoot.getClipToPadding());
        assertTrue(mContentRoot.getClipChildren());
        assertTrue(mContentRoot.getClipToPadding());
    }

    @Test
    public void testSetSheetContainerForTesting_RestoresPreviousContainerAncestors() {
        mOuterRoot.setClipChildren(true);
        mOuterRoot.setClipToPadding(true);
        mContentRoot.setClipChildren(true);
        mContentRoot.setClipToPadding(false);
        mDelegate.setAncestorsUnclipped(true);

        FrameLayout newParent = new FrameLayout(mContext);
        newParent.setClipChildren(true);
        newParent.setClipToPadding(true);
        FrameLayout newContainer = new FrameLayout(mContext);
        newContainer.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        newParent.addView(newContainer);

        mDelegate.setSheetContainerForTesting(newContainer);
        assertTrue(mOuterRoot.getClipChildren());
        assertTrue(mOuterRoot.getClipToPadding());
        assertTrue(mContentRoot.getClipChildren());
        assertFalse(mContentRoot.getClipToPadding());

        // Re-setting the same container is a no-op; subsequent updates target newContainer.
        mDelegate.setSheetContainerForTesting(newContainer);
        mDelegate.setContainerBottomMargin(24);
        mDelegate.setAncestorsUnclipped(true);
        assertEquals(24, ((MarginLayoutParams) newContainer.getLayoutParams()).bottomMargin);
        assertEquals(0, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertFalse(newParent.getClipChildren());
        assertFalse(newParent.getClipToPadding());
        assertTrue(mOuterRoot.getClipChildren());
    }
}

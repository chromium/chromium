// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the {@link SelectionDelegate} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionDelegateTest {
    private final Object mData1 = new Object();
    private final Object mData2 = new Object();

    @Test
    public void testSelectionDelegateSingle() {
        SelectionDelegate<Object> delegate = new SelectionDelegate<Object>();
        delegate.setSingleSelectionMode();

        // Starting state, nothing is selected.
        assertFalse(delegate.isItemSelected(mData1));
        assertFalse(delegate.isSelectionEnabled());

        // Select first item and verify selection.
        delegate.toggleSelectionForItem(mData1);
        assertTrue(delegate.isItemSelected(mData1));
        assertTrue(delegate.isSelectionEnabled());

        // Select second item, first item gets unselected.
        delegate.toggleSelectionForItem(mData2);
        assertFalse(delegate.isItemSelected(mData1));
        assertTrue(delegate.isItemSelected(mData2));
        assertTrue(delegate.isSelectionEnabled());

        // Unselect second item.
        delegate.toggleSelectionForItem(mData2);
        assertFalse(delegate.isItemSelected(mData2));
        assertFalse(delegate.isSelectionEnabled());

        // Test clearing selection.
        delegate.toggleSelectionForItem(mData1);
        delegate.clearSelection();
        assertFalse(delegate.isItemSelected(mData2));
        assertFalse(delegate.isSelectionEnabled());
    }

    @Test
    public void testSelectionDelegateMulti() {
        SelectionDelegate<Object> delegate = new SelectionDelegate<Object>();

        // Starting state, nothing is selected.
        assertFalse(delegate.isItemSelected(mData1));
        assertFalse(delegate.isSelectionEnabled());

        // Select first item and verify selection.
        delegate.toggleSelectionForItem(mData1);
        assertTrue(delegate.isItemSelected(mData1));
        assertTrue(delegate.isSelectionEnabled());

        // Select second item, first item does not get unselected.
        delegate.toggleSelectionForItem(mData2);
        assertTrue(delegate.isItemSelected(mData1));
        assertTrue(delegate.isItemSelected(mData2));
        assertTrue(delegate.isSelectionEnabled());

        // Unselect second item.
        delegate.toggleSelectionForItem(mData2);
        assertFalse(delegate.isItemSelected(mData2));
        assertTrue(delegate.isItemSelected(mData1));
        assertTrue(delegate.isSelectionEnabled());

        // Test clearing selection.
        delegate.clearSelection();
        assertFalse(delegate.isItemSelected(mData1));
        assertFalse(delegate.isItemSelected(mData2));
        assertFalse(delegate.isSelectionEnabled());
    }
}

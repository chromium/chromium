// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExtensionActionListAnchoredModelList}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionActionListAnchoredModelListTest {
    private ModelList mSourceList;
    private ExtensionActionListAnchoredModelList mAnchoredList;

    @Before
    public void setUp() {
        mSourceList = new ModelList();

        // Initialize with one item to test constructor logic.
        mSourceList.add(createListItem("item1"));

        mAnchoredList = new ExtensionActionListAnchoredModelList(mSourceList);
    }

    @Test
    public void testInitialState() {
        // Anchored list should be [Anchor, item1, Anchor].
        assertEquals(3, mAnchoredList.size());
        assertEquals(ListItemType.ANCHOR, mAnchoredList.get(0).type);
        assertEquals("item1", mAnchoredList.get(1).model.get(ExtensionActionButtonProperties.ID));
        assertEquals(ListItemType.ANCHOR, mAnchoredList.get(2).type);
    }

    @Test
    public void testSourceAddition() {
        mSourceList.add(createListItem("item2"));

        // Anchored list should be [Anchor, item1, item2, Anchor].
        assertEquals(4, mAnchoredList.size());
        assertEquals("item2", mAnchoredList.get(2).model.get(ExtensionActionButtonProperties.ID));
        assertEquals(ListItemType.ANCHOR, mAnchoredList.get(3).type);
    }

    @Test
    public void testSourceRemoval() {
        mSourceList.removeAt(0);

        // Anchored list should be [Anchor, Anchor].
        assertEquals(2, mAnchoredList.size());
        assertEquals(ListItemType.ANCHOR, mAnchoredList.get(0).type);
        assertEquals(ListItemType.ANCHOR, mAnchoredList.get(1).type);
    }

    @Test
    public void testSourceMove() {
        // Make the list [item1, item2].
        mSourceList.add(createListItem("item2"));

        // Move items so that the list is [item2, item1].
        mSourceList.move(0, 1);

        // Anchored list should be [Anchor, item2, item1, Anchor].
        assertEquals("item2", mAnchoredList.get(1).model.get(ExtensionActionButtonProperties.ID));
        assertEquals("item1", mAnchoredList.get(2).model.get(ExtensionActionButtonProperties.ID));
    }

    @Test
    public void testAnchoredMovePropagatesToSource() {
        // Make the list [item1, item2].
        mSourceList.add(createListItem("item2"));

        // Move item1 (index 1) to after item2 (index 2) in the anchored list.
        mAnchoredList.move(1, 2);

        // Verify source list also moved to become [item2, item1].
        assertEquals("item2", mSourceList.get(0).model.get(ExtensionActionButtonProperties.ID));
        assertEquals("item1", mSourceList.get(1).model.get(ExtensionActionButtonProperties.ID));
    }

    @Test
    public void testGetIndexForActionId() {
        mSourceList.add(createListItem("item2"));

        // "item1" is at index 1 in anchored list.
        assertEquals(1, mAnchoredList.getIndexForActionId("item1"));

        // "item2" is at index 2 in anchored list.
        assertEquals(2, mAnchoredList.getIndexForActionId("item2"));

        // Non-existent ID.
        assertEquals(-1, mAnchoredList.getIndexForActionId("fake_id"));
    }

    private ListItem createListItem(String id) {
        PropertyModel model =
                new PropertyModel.Builder(ExtensionActionButtonProperties.ALL_KEYS)
                        .with(ExtensionActionButtonProperties.ID, id)
                        .build();
        return new ListItem(ListItemType.EXTENSION_ACTION, model);
    }
}

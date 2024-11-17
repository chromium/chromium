// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/** Tests for the {@link SelectableItemViewBase} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectableItemViewBaseTest {

    private static class SelectableItemViewBaseTestImpl<E> extends SelectableItemViewBase {
        private final CallbackHelper mHandleNonSelectionClickHelper;

        public SelectableItemViewBaseTestImpl(
                Context context, AttributeSet attrs, CallbackHelper handleNonSelectionClickHelper) {
            super(context, attrs);
            mHandleNonSelectionClickHelper = handleNonSelectionClickHelper;
        }

        @Override
        protected void handleNonSelectionClick() {
            mHandleNonSelectionClickHelper.notifyCalled();
        }
    }

    private SelectableItemViewBaseTestImpl<Integer> mSelectableItemViewBase;
    private CallbackHelper mHandleNonSelectionClickHelper;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mHandleNonSelectionClickHelper = new CallbackHelper();
        mSelectableItemViewBase =
                new SelectableItemViewBaseTestImpl<Integer>(
                        activity, /* attrs= */ null, mHandleNonSelectionClickHelper);
    }

    @Test
    public void testSelection() throws TimeoutException {
        SelectionDelegate<Integer> selectionDelegate = new SelectionDelegate<>();
        mSelectableItemViewBase.setSelectionDelegate(selectionDelegate);

        Integer item = 1;
        assertNull(mSelectableItemViewBase.getItem());
        mSelectableItemViewBase.setItem(item);
        assertEquals(item, mSelectableItemViewBase.getItem());

        // Verify that toggling selection in the delegate does the same to the view.
        assertFalse(mSelectableItemViewBase.isChecked());
        selectionDelegate.toggleSelectionForItem(item);
        assertTrue(mSelectableItemViewBase.isChecked());

        assertTrue(selectionDelegate.isSelectionEnabled());
        mSelectableItemViewBase.onClick(mSelectableItemViewBase);
        assertFalse(mSelectableItemViewBase.isChecked());
        assertFalse(selectionDelegate.isItemSelected(item));

        // Nothing is selected, so selection shouldn't be enabled.
        assertFalse(selectionDelegate.isSelectionEnabled());
        // When selection is disabled, click events should instead call the
        // `handleNonSelectionClick` method instead of selecting the item.
        mSelectableItemViewBase.onClick(mSelectableItemViewBase);
        assertFalse(mSelectableItemViewBase.isChecked());
        mHandleNonSelectionClickHelper.waitForOnly();
    }

    @Test
    public void testSelection_NullDelegate() {
        Integer item = 1;
        assertNull(mSelectableItemViewBase.getItem());
        mSelectableItemViewBase.setItem(item);
        assertNull(mSelectableItemViewBase.getItem());

        mSelectableItemViewBase.onClick(mSelectableItemViewBase);
        assertFalse(mSelectableItemViewBase.isChecked());
        // Even though selection mode isn't enabled, `handleNonSelectionClick` isn't called. The
        // entire onClick method is short-circuit because SelectionDelegate is null.
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Integration tests for {@link AppMenuListView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AppMenuListViewTest {

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private AppMenuListView setUpAppMenuListView() {
        // We need list selection; ensure we are not in touch mode.
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);

        Activity testActivity = mActivityRule.launchActivity(/* startIntent= */ null);

        AppMenuListView listView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AppMenuListView view =
                                    new AppMenuListView(testActivity, /* attrs= */ null);
                            view.setItemsCanFocus(true);
                            view.setFocusableInTouchMode(true);
                            testActivity.setContentView(view);

                            // Create an adapter that returns a composite row with multiple
                            // focusable views.
                            ArrayAdapter<String> adapter =
                                    new ArrayAdapter<String>(
                                            testActivity,
                                            0,
                                            new String[] {"Row 0", "Row 1", "Row 2"}) {
                                        @Override
                                        public View getView(
                                                int position, View convertView, ViewGroup parent) {
                                            LinearLayout row = new LinearLayout(getContext());
                                            // Make sure Android knows it's a list item row block
                                            row.setDescendantFocusability(
                                                    ViewGroup.FOCUS_AFTER_DESCENDANTS);
                                            row.setFocusableInTouchMode(true);

                                            Button b1 = new Button(getContext());
                                            b1.setFocusable(true);
                                            b1.setFocusableInTouchMode(true);
                                            b1.setContentDescription("Button 1 - Row " + position);

                                            Button b2 = new Button(getContext());
                                            b2.setFocusable(true);
                                            b2.setFocusableInTouchMode(true);
                                            b2.setContentDescription("Button 2 - Row " + position);

                                            row.addView(b1);
                                            row.addView(b2);
                                            return row;
                                        }
                                    };
                            view.setAdapter(adapter);
                            return view;
                        });

        CriteriaHelper.pollUiThread(() -> listView.getChildCount() > 0);
        return listView;
    }

    private void setupRowFocusability(ViewGroup row) {
        row.setFocusable(true);
        row.setFocusableInTouchMode(true);
        row.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_TabBetweenSiblingsInRow() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Focus the first button in the first row
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);
                    View button2 = rowGroup.getChildAt(1);

                    button1.requestFocus();
                    assertTrue(button1.isFocused());

                    // Act: Simulate a TAB key event
                    KeyEvent tabEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
                    boolean handled = listView.dispatchKeyEvent(tabEvent);

                    // Assert: We intercepted it internally and successfully shifted focus to
                    // button2
                    assertTrue(
                            "AppMenuListView should have intercepted the horizontal TAB.", handled);
                    assertTrue(
                            "Focus should have moved to the second button in the same row.",
                            button2.isFocused());
                    assertEquals(
                            "Selected list position should be synced to row 0.",
                            0,
                            listView.getSelectedItemPosition());
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_ShiftTabBetweenSiblingsInRow() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Focus the second button in the first row
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);
                    View button2 = rowGroup.getChildAt(1);

                    button2.requestFocus();
                    assertTrue(button2.isFocused());

                    // Act: Simulate a SHIFT+TAB key event (FOCUS_BACKWARD)
                    KeyEvent shiftTabEvent =
                            new KeyEvent(
                                    0,
                                    0,
                                    KeyEvent.ACTION_DOWN,
                                    KeyEvent.KEYCODE_TAB,
                                    0,
                                    KeyEvent.META_SHIFT_ON);
                    boolean handled = listView.dispatchKeyEvent(shiftTabEvent);

                    // Assert: We intercepted it internally and successfully shifted focus to
                    // button1
                    assertTrue(
                            "AppMenuListView should have intercepted the horizontal SHIFT+TAB.",
                            handled);
                    assertTrue(
                            "Focus should have moved to the first button in the same row.",
                            button1.isFocused());
                    assertEquals(
                            "Selected list position should be synced to row 0.",
                            0,
                            listView.getSelectedItemPosition());
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_TabCrossesRowForward() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Focus the SECOND button in the FIRST row
                    ViewGroup row0 = (ViewGroup) listView.getChildAt(0);
                    View button2Row0 = row0.getChildAt(1);

                    ViewGroup row1 = (ViewGroup) listView.getChildAt(1);
                    View button1Row1 = row1.getChildAt(0);

                    // Artificially enforce focus routing in the test hierarchy to simulate
                    // Android's native spatial layout FocusFinder evaluation on real devices.
                    button2Row0.setId(View.generateViewId());
                    button1Row1.setId(View.generateViewId());
                    button2Row0.setNextFocusForwardId(button1Row1.getId());

                    button2Row0.requestFocus();
                    assertTrue(button2Row0.isFocused());

                    // Act: Simulate a TAB key event (FOCUS_FORWARD)
                    KeyEvent tabEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
                    boolean handled = listView.dispatchKeyEvent(tabEvent);

                    // Assert: We intercepted it because nextFocus is a nested child element.
                    assertTrue(
                            "AppMenuListView should intercept horizontal TAB leaving the row"
                                    + " forward.",
                            handled);
                    assertTrue(
                            "Focus should have moved to the adjacent row.",
                            button1Row1.isFocused());
                    assertEquals(
                            "Selected list position should be synced to row 1.",
                            1,
                            listView.getSelectedItemPosition());
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_ShiftTabCrossesRowBackward() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Focus the FIRST button in the SECOND row
                    ViewGroup row0 = (ViewGroup) listView.getChildAt(0);
                    View button2Row0 = row0.getChildAt(1);

                    ViewGroup row1 = (ViewGroup) listView.getChildAt(1);
                    View button1Row1 = row1.getChildAt(0);

                    button1Row1.requestFocus();
                    assertTrue(button1Row1.isFocused());

                    // Act: Simulate a SHIFT+TAB key event (FOCUS_BACKWARD)
                    KeyEvent shiftTabEvent =
                            new KeyEvent(
                                    0,
                                    0,
                                    KeyEvent.ACTION_DOWN,
                                    KeyEvent.KEYCODE_TAB,
                                    0,
                                    KeyEvent.META_SHIFT_ON);
                    boolean handled = listView.dispatchKeyEvent(shiftTabEvent);

                    // Assert: We intercepted it because nextFocus is a nested child element.
                    assertTrue(
                            "AppMenuListView should intercept horizontal TAB leaving the row"
                                    + " backward.",
                            handled);
                    assertTrue(
                            "Focus should have moved to the adjacent row's last button.",
                            button2Row0.isFocused());
                    assertEquals(
                            "Selected list position should be synced to row 0.",
                            0,
                            listView.getSelectedItemPosition());
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_TabToDirectRow_HandledByNativeDispatch() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Focus the SECOND button in the FIRST row
                    ViewGroup row0 = (ViewGroup) listView.getChildAt(0);
                    View button2Row0 = row0.getChildAt(1);

                    ViewGroup row1 = (ViewGroup) listView.getChildAt(1);
                    // Make the entire row a monolithic focusable entity and route focus to it
                    setupRowFocusability(row1);

                    button2Row0.setId(View.generateViewId());
                    row1.setId(View.generateViewId());
                    button2Row0.setNextFocusForwardId(row1.getId());

                    button2Row0.requestFocus();
                    assertTrue(button2Row0.isFocused());

                    // Verify that nextFocus is a direct child of ListView
                    assertEquals(listView, row1.getParent());

                    // Act: Simulate a TAB key event (FOCUS_FORWARD)
                    KeyEvent tabEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
                    boolean handled = listView.dispatchKeyEvent(tabEvent);

                    // Assert: AppMenuListView does not intercept direct rows with custom logic,
                    // letting Android's native dispatch handle the TAB event.
                    assertTrue(
                            "AppMenuListView should delegate TAB to direct monolithic row to native"
                                    + " dispatch.",
                            handled);
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_ShiftTabToDirectRow_HandledByNativeDispatch() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Make the entire row 0 a monolithic focusable entity
                    ViewGroup row0 = (ViewGroup) listView.getChildAt(0);
                    setupRowFocusability(row0);
                    row0.setId(View.generateViewId());

                    // Focus the FIRST button in the SECOND row
                    ViewGroup row1 = (ViewGroup) listView.getChildAt(1);
                    View button1Row1 = row1.getChildAt(0);

                    button1Row1.requestFocus();
                    assertTrue(button1Row1.isFocused());

                    // Verify that nextFocus target (row0) is a direct child of ListView
                    assertEquals(listView, row0.getParent());

                    // Act: Simulate a SHIFT+TAB key event (FOCUS_BACKWARD)
                    KeyEvent shiftTabEvent =
                            new KeyEvent(
                                    0,
                                    0,
                                    KeyEvent.ACTION_DOWN,
                                    KeyEvent.KEYCODE_TAB,
                                    0,
                                    KeyEvent.META_SHIFT_ON);
                    boolean handled = listView.dispatchKeyEvent(shiftTabEvent);

                    // Assert: AppMenuListView does not intercept direct rows with custom logic,
                    // letting Android's native dispatch handle the SHIFT+TAB event.
                    assertTrue(
                            "AppMenuListView should delegate SHIFT+TAB to direct monolithic row to"
                                    + " native dispatch.",
                            handled);
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_TabActionUp_HandledByNativeDispatch() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);

                    button1.requestFocus();
                    assertTrue(button1.isFocused());

                    // Act: Simulate a TAB key UP event (should not trigger ACTION_DOWN interception
                    // logic)
                    KeyEvent tabUpEvent = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_TAB);
                    boolean handled = listView.dispatchKeyEvent(tabUpEvent);

                    // Assert: ACTION_UP is delegated to super (which does not consume TAB key up)
                    assertFalse(
                            "ACTION_UP should be delegated to super and not consumed.", handled);
                    assertTrue("Focus should remain on button1 on key up.", button1.isFocused());
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_CtrlTab_HandledByNativeDispatch() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);

                    button1.requestFocus();
                    assertTrue(button1.isFocused());

                    // Act: Simulate a Ctrl+TAB key event (unsupported modifier for menu navigation)
                    KeyEvent ctrlTabEvent =
                            new KeyEvent(
                                    0,
                                    0,
                                    KeyEvent.ACTION_DOWN,
                                    KeyEvent.KEYCODE_TAB,
                                    0,
                                    KeyEvent.META_CTRL_ON);
                    boolean handled = listView.dispatchKeyEvent(ctrlTabEvent);

                    // Assert: Ctrl+TAB is not intercepted by custom horizontal navigation,
                    // and native ListView does not handle Ctrl+TAB.
                    assertFalse("Ctrl+TAB should be delegated to super and not consumed.", handled);
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_UnhandledNonTabKey_ReturnsFalse() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);

                    button1.requestFocus();
                    assertTrue(button1.isFocused());

                    // Act: Simulate an unhandled key event (e.g. KEYCODE_A)
                    KeyEvent unhandledEvent =
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
                    boolean handled = listView.dispatchKeyEvent(unhandledEvent);

                    // Assert: Unhandled non-navigation keys return false.
                    assertFalse(
                            "AppMenuListView should return false for unhandled non-navigation"
                                    + " keys.",
                            handled);
                });
    }

    @Test
    @SmallTest
    public void testDispatchKeyEvent_DpadKey_HandledByNativeDispatch() {
        AppMenuListView listView = setUpAppMenuListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View firstRow = listView.getChildAt(0);
                    assertNotNull(firstRow);

                    ViewGroup rowGroup = (ViewGroup) firstRow;
                    View button1 = rowGroup.getChildAt(0);

                    button1.requestFocus();
                    assertTrue(button1.isFocused());

                    // Act: Simulate a DPAD key event
                    KeyEvent dpadDownEvent =
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN);
                    boolean handled = listView.dispatchKeyEvent(dpadDownEvent);

                    // Assert: DPAD navigation is handled natively by ListView.
                    assertTrue("DPAD navigation should be handled by native ListView.", handled);
                });
    }
}

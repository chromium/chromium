// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridgeJni;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Tests for {@link ExtensionsMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ExtensionsMenuMediatorTest {
    // Static inner class to hold property keys for the test. In a real scenario, this would
    // be a public class.
    private static final int TAB1_ID = 111;
    private static final int TAB2_ID = 222;
    private static final long ACTIONS_BRIDGE_POINTER = 10000L;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = createSimpleIcon(Color.GREEN);
    private static final Bitmap ICON_CYAN = createSimpleIcon(Color.CYAN);
    private static final Bitmap ICON_MAGENTA = createSimpleIcon(Color.MAGENTA);
    private static final Bitmap ICON_YELLOW = createSimpleIcon(Color.YELLOW);
    private static final Bitmap ICON_WHITE = createSimpleIcon(Color.WHITE);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private ExtensionActionsBridge.Natives mActionsBridgeJniMock;
    @Mock private Runnable mDataReadyCallback;

    private ExtensionActionsBridge mActionsBridge;
    private MockTab mTab1;
    private MockTab mTab2;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<Tab> mCurrentTabSupplier;
    private ModelList mModels;
    private ExtensionsMenuMediator mMediator;

    @Before
    public void setUp() {
        ExtensionActionsBridgeJni.setInstanceForTesting(mActionsBridgeJniMock);

        // Provide good defaults for action queries via JNI.
        mActionsBridge = new ExtensionActionsBridge(ACTIONS_BRIDGE_POINTER);
        when(mActionsBridgeJniMock.get(mProfile)).thenReturn(mActionsBridge);
        when(mActionsBridgeJniMock.areActionsInitialized(ACTIONS_BRIDGE_POINTER)).thenReturn(true);
        when(mActionsBridgeJniMock.getActionIds(ACTIONS_BRIDGE_POINTER))
                .thenReturn(new String[] {"a", "b"});
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "a", TAB1_ID))
                .thenReturn(new ExtensionAction("a", "title of a"));
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "b", TAB1_ID))
                .thenReturn(new ExtensionAction("b", "title of b"));
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "c", TAB1_ID))
                .thenReturn(new ExtensionAction("c", "title of c"));
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "a", TAB2_ID))
                .thenReturn(new ExtensionAction("a", "another title of a"));
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "b", TAB2_ID))
                .thenReturn(new ExtensionAction("b", "another title of b"));
        when(mActionsBridgeJniMock.getAction(ACTIONS_BRIDGE_POINTER, "c", TAB2_ID))
                .thenReturn(new ExtensionAction("c", "another title of c"));
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "a", TAB1_ID))
                .thenReturn(ICON_RED);
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "b", TAB1_ID))
                .thenReturn(ICON_GREEN);
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "c", TAB1_ID))
                .thenReturn(ICON_BLUE);
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "a", TAB2_ID))
                .thenReturn(ICON_CYAN);
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "b", TAB2_ID))
                .thenReturn(ICON_MAGENTA);
        when(mActionsBridgeJniMock.getActionIcon(ACTIONS_BRIDGE_POINTER, "c", TAB2_ID))
                .thenReturn(ICON_YELLOW);

        // Initialize common objects.
        mTab1 = new MockTab(TAB1_ID, mProfile);
        mTab2 = new MockTab(TAB2_ID, mProfile);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mCurrentTabSupplier = new ObservableSupplierImpl<>();
        mModels = new ModelList();
        mMediator =
                new ExtensionsMenuMediator(
                        mProfileSupplier, mCurrentTabSupplier, mModels, mDataReadyCallback);

        // Wait for the main thread to settle.
        shadowOf(Looper.getMainLooper()).idle();
    }

    @After
    public void tearDown() {
        mMediator.destroy();
    }

    @Test
    public void testUpdateModels() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "title of a", ICON_RED);
        assertItemAt(1, "title of b", ICON_GREEN);
    }

    @Test
    public void testUpdateModels_dataReadyCallback() {
        // The callback should not have been called.
        verify(mDataReadyCallback, never()).run();

        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The callback should have been called.
        verify(mDataReadyCallback).run();
    }

    @Test
    public void testUpdateModels_noProfile() {
        // Set the tab only.
        mCurrentTabSupplier.set(mTab1);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());
        verify(mActionsBridgeJniMock, never()).get(any());
    }

    @Test
    public void testUpdateModels_noTab() {
        // Set the profile only.
        mProfileSupplier.set(mProfile);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());
        verify(mActionsBridgeJniMock, never()).getActionIds(anyLong());
    }

    @Test
    public void testUpdateModels_tabChanged() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "title of a", ICON_RED);
        assertItemAt(1, "title of b", ICON_GREEN);

        // Simulate changing the tab.
        mCurrentTabSupplier.set(mTab2);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "another title of a", ICON_CYAN);
        assertItemAt(1, "another title of b", ICON_MAGENTA);
    }

    private static Bitmap createSimpleIcon(int color) {
        Bitmap bitmap = Bitmap.createBitmap(12, 12, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColor(color);
        canvas.drawRect(0, 0, 12, 12, paint);
        return bitmap;
    }

    private void assertItemAt(int index, String title, Bitmap icon) {
        ListItem item = mModels.get(index);
        assertEquals(0, item.type);
        assertEquals(title, item.model.get(ExtensionsMenuItemProperties.TITLE));
        assertTrue(icon.sameAs(item.model.get(ExtensionsMenuItemProperties.ICON)));
    }
}

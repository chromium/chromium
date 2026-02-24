// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Looper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.HashMap;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionActionListMediatorTest {

    /** An representation of an extension action. */
    private static class ActionData {
        private final String mId;
        private final String mTitle;
        private final Bitmap mIcon;

        public ActionData(String id, String title, Bitmap icon) {
            mId = id;
            mTitle = title;
            mIcon = icon;
        }

        public String getId() {
            return mId;
        }

        public String getTitle() {
            return mTitle;
        }

        public Bitmap getIcon() {
            return mIcon;
        }
    }

    private static final int TAB_ID = 111;
    private static final long BROWSER_WINDOW_POINTER = 1000L;
    private static final long ACTION_CONTEXT_MENU_BRIDGE_POINTER = 10000L;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = createSimpleIcon(Color.GREEN);
    private static final Bitmap ICON_CYAN = createSimpleIcon(Color.CYAN);
    private static final Bitmap ICON_MAGENTA = createSimpleIcon(Color.MAGENTA);

    private static final String ACTION1_ID = "aaaaa";
    private static final String ACTION2_ID = "bbbbb";
    private static final String ACTION3_ID = "ccccc";

    private final Map<String, ActionData> mActions = new HashMap<>();

    private ExtensionActionListMediator mMediator;
    private ModelList mModels;
    private MockTab mTab;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WebContents mWebContents;
    @Mock private ExtensionActionContextMenuBridge.Native mActionContextMenuBridgeJniMock;
    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private MenuModelBridge mMenuModelBridge;
    @Mock private ExtensionActionListCoordinator.ActionAnchorViewProvider mActionAnchorViewProvider;

    @Captor private ArgumentCaptor<ListMenuHost.PopupMenuShownListener> mPopupListenerCaptor;

    @Captor private ArgumentCaptor<ExtensionsToolbarBridge.Delegate> mBridgeDelegateCaptor;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();

        // Mock AndroidChromeTask.
        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);

        // Mock JNI for Context Menu Bridge.
        ExtensionActionContextMenuBridgeJni.setInstanceForTesting(mActionContextMenuBridgeJniMock);
        when(mActionContextMenuBridgeJniMock.init(anyLong(), any(), any(), anyInt()))
                .thenReturn(ACTION_CONTEXT_MENU_BRIDGE_POINTER);
        when(mActionContextMenuBridgeJniMock.getMenuModelBridge(anyLong()))
                .thenReturn(mMenuModelBridge);
        when(mMenuModelBridge.populateModelList()).thenReturn(new ModelList());

        // Set up default actions.
        ActionData action1 = new ActionData(ACTION1_ID, "title of action 1", ICON_RED);
        ActionData action2 = new ActionData(ACTION2_ID, "title of action 2", ICON_BLUE);
        ActionData action3 = new ActionData(ACTION3_ID, "title of action 3", ICON_GREEN);

        mActions.put(ACTION1_ID, action1);
        mActions.put(ACTION2_ID, action2);
        mActions.put(ACTION3_ID, action3);

        when(mExtensionsToolbarBridge.getAction(anyString()))
                .thenAnswer(
                        invocation -> {
                            String id = invocation.getArgument(0);

                            ActionData action = mActions.get(id);
                            assert action != null;

                            return new ExtensionAction(action.getId(), action.getTitle());
                        });

        when(mExtensionsToolbarBridge.getPinnedActionIds())
                .thenReturn(new String[] {ACTION1_ID, ACTION2_ID});

        // Initialize common objects.
        mTab = new MockTab(TAB_ID, mProfile);
        mTab.setWebContentsOverrideForTesting(mWebContents);
        mCurrentTabSupplier = ObservableSuppliers.createNullable(mTab);

        mModels = new ModelList();

        mMediator =
                new ExtensionActionListMediator(
                        context,
                        mWindowAndroid,
                        mModels,
                        mTask,
                        mProfile,
                        mCurrentTabSupplier,
                        mActionAnchorViewProvider,
                        mExtensionsToolbarBridge) {
                    @Override
                    Bitmap getIconForAction(String actionId, WebContents webContents) {
                        ActionData action = mActions.get(actionId);
                        assert action != null;

                        return action.getIcon();
                    }
                };

        mMediator.fitActionsWithinWidth(1000);
        verify(mExtensionsToolbarBridge).setDelegate(mBridgeDelegateCaptor.capture());

        shadowOf(Looper.getMainLooper()).idle();
    }

    @After
    public void tearDown() {
        mMediator.destroy();
    }

    @Test
    public void testUpdateModels_onInitialized() {
        mMediator.reconcileActionItems();

        // The models should be updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);

        ListItem item = mModels.get(0);
        assertNotNull(
                "Click listener should be set",
                item.model.get(ExtensionActionButtonProperties.ON_CLICK_LISTENER));
        assertNotNull(
                "Long click listener should be set",
                item.model.get(ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER));
    }

    @Test
    public void testUpdateModels_onActionAddedOrRemovedWithReconcile() {
        mMediator.reconcileActionItems();

        // The models should be updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);

        // Save the {@link ListItem} instances.
        ListItem itemForAction1 = mModels.get(0);
        ListItem itemForAction2 = mModels.get(1);

        // Add action 3 to the list of IDs.
        when(mExtensionsToolbarBridge.getPinnedActionIds())
                .thenReturn(new String[] {ACTION1_ID, ACTION2_ID, ACTION3_ID});
        mMediator.reconcileActionItems();

        // The models should have the additional item.
        assertEquals(3, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);
        assertItemAt(2, ACTION3_ID, "title of action 3", ICON_GREEN);

        // Save the {@link ListItem} instance.
        ListItem itemForAction3 = mModels.get(2);

        // The same models should be used for existing actions.
        assertSame("The item object should be reused", itemForAction1, mModels.get(0));
        assertSame("The item object should be reused", itemForAction2, mModels.get(1));

        // Remove action 2 from the list of IDs.
        when(mExtensionsToolbarBridge.getPinnedActionIds())
                .thenReturn(new String[] {ACTION1_ID, ACTION3_ID});
        mMediator.reconcileActionItems();

        // The models should have 2 items.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION3_ID, "title of action 3", ICON_GREEN);

        // The same models should be used for existing actions.
        assertSame("The item object should be reused", itemForAction1, mModels.get(0));
        assertSame("The item object should be reused", itemForAction3, mModels.get(1));
    }

    @Test
    public void testUpdateModels_onActionMoved() {
        mMediator.reconcileActionItems();

        // The models should be updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);

        // Save the {@link ListItem} instances.
        ListItem itemForAction1 = mModels.get(0);
        ListItem itemForAction2 = mModels.get(1);

        // Swap action 1 and action 2.
        when(mExtensionsToolbarBridge.getPinnedActionIds())
                .thenReturn(new String[] {ACTION2_ID, ACTION1_ID});
        mMediator.reconcileActionItems();

        // The models should have the additional item.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION2_ID, "title of action 2", ICON_BLUE);
        assertItemAt(1, ACTION1_ID, "title of action 1", ICON_RED);

        // The same models should be used for existing actions.
        assertSame("The item object should be reused", itemForAction2, mModels.get(0));
        assertSame("The item object should be reused", itemForAction1, mModels.get(1));
    }

    @Test
    public void testUpdateModels_onActionUpdated() {
        mMediator.reconcileActionItems();

        // The models should be updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);

        // Save the {@link ListItem} instances.
        ListItem itemForAction1 = mModels.get(0);
        ListItem itemForAction2 = mModels.get(1);

        mActions.put(ACTION1_ID, new ActionData(ACTION1_ID, "new title of action 1", ICON_CYAN));
        mMediator.updateActionProperties(ACTION1_ID);

        // The models should have the additional item.
        assertEquals(2, mModels.size());
        assertItemAt(0, ACTION1_ID, "new title of action 1", ICON_CYAN);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);

        // The same models should be used for existing actions.
        assertSame("The item object should be reused", itemForAction1, mModels.get(0));
        assertSame("The item object should be reused", itemForAction2, mModels.get(1));
    }

    @Test
    public void testFitActionsWithinWidth_HidesExtraItems() {
        mMediator.reconcileActionItems();

        // The models should be updated.
        assertEquals(2, mModels.size());

        Context context = ApplicationProvider.getApplicationContext();
        int itemWidth =
                context.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.toolbar.R.dimen.toolbar_button_width);

        // Test ample width.
        mMediator.fitActionsWithinWidth(itemWidth * 5);
        assertEquals(2, mModels.size());

        // Test width for 2 items.
        mMediator.fitActionsWithinWidth(itemWidth * 2);
        assertEquals(2, mModels.size());

        // Test width for more than 1 item but less than 2 items.
        mMediator.fitActionsWithinWidth(itemWidth + (int) (itemWidth / 2));
        assertEquals(1, mModels.size());

        // Test width for exactly 1 item.
        mMediator.fitActionsWithinWidth(itemWidth);
        assertEquals(1, mModels.size());

        // Test width insufficient for 1 item.
        mMediator.fitActionsWithinWidth(itemWidth - 1);

        // There should be 0 items.
        assertEquals(0, mModels.size());
    }

    private static Bitmap createSimpleIcon(int color) {
        Bitmap bitmap = Bitmap.createBitmap(12, 12, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColor(color);
        canvas.drawRect(0, 0, 12, 12, paint);
        return bitmap;
    }

    private void assertItemAt(int index, String id, String title, Bitmap icon) {
        ListItem item = mModels.get(index);
        assertEquals(ListItemType.EXTENSION_ACTION, item.type);
        assertEquals(id, item.model.get(ExtensionActionButtonProperties.ID));
        assertEquals(title, item.model.get(ExtensionActionButtonProperties.TITLE));
        assertTrue(icon.sameAs(item.model.get(ExtensionActionButtonProperties.ICON)));
    }
}

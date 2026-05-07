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
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction.HoverCardState;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContentsJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.toolbar.AdminPolicy;
import org.chromium.chrome.browser.ui.toolbar.SiteAccess;
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
        private final HoverCardState mHoverCardState;

        public ActionData(String id, String title, Bitmap icon, HoverCardState hoverCardState) {
            mId = id;
            mTitle = title;
            mIcon = icon;
            mHoverCardState = hoverCardState;
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

        public HoverCardState getHoverCardState() {
            return mHoverCardState;
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

    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;

    @Mock private ExtensionActionPopupContents mPopupContentsMock;
    @Mock private ExtensionActionPopupContents.Natives mPopupContentsJniMock;

    @Mock private MenuModelBridge mMenuModelBridge;
    @Mock private ExtensionActionContextMenuBridge.Native mActionContextMenuBridgeJniMock;

    @Mock private ExtensionActionListCoordinator.RecyclerViewDelegate mRecyclerViewDelegate;

    @Mock private TabModelSelector mTabModelSelector;

    @Captor private ArgumentCaptor<ListMenuHost.PopupMenuShownListener> mPopupListenerCaptor;

    @Captor
    private ArgumentCaptor<ExtensionsToolbarBridge.ActionListDelegate> mBridgeDelegateCaptor;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();

        // Mock AndroidChromeTask.
        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);

        // Add the JNI mock for ExtensionActionPopupContents:
        ExtensionActionPopupContentsJni.setInstanceForTesting(mPopupContentsJniMock);
        when(mPopupContentsJniMock.create(anyLong())).thenReturn(mPopupContentsMock);

        // Mock JNI for Context Menu Bridge.
        ExtensionActionContextMenuBridgeJni.setInstanceForTesting(mActionContextMenuBridgeJniMock);
        when(mActionContextMenuBridgeJniMock.init(anyLong(), any(), any(), anyInt()))
                .thenReturn(ACTION_CONTEXT_MENU_BRIDGE_POINTER);
        when(mActionContextMenuBridgeJniMock.getMenuModelBridge(anyLong()))
                .thenReturn(mMenuModelBridge);
        when(mMenuModelBridge.populateModelList()).thenReturn(new ModelList());

        HoverCardState hoverCardState =
                new HoverCardState(SiteAccess.ALL_EXTENSIONS_ALLOWED, "", "", AdminPolicy.NONE, "");

        // Set up default actions.
        ActionData action1 =
                new ActionData(ACTION1_ID, "title of action 1", ICON_RED, hoverCardState);
        ActionData action2 =
                new ActionData(ACTION2_ID, "title of action 2", ICON_BLUE, hoverCardState);
        ActionData action3 =
                new ActionData(ACTION3_ID, "title of action 3", ICON_GREEN, hoverCardState);

        mActions.put(ACTION1_ID, action1);
        mActions.put(ACTION2_ID, action2);
        mActions.put(ACTION3_ID, action3);

        when(mExtensionsToolbarBridge.getAction(anyString(), any(WebContents.class)))
                .thenAnswer(
                        invocation -> {
                            String id = invocation.getArgument(0);

                            ActionData action = mActions.get(id);
                            assert action != null;

                            return new ExtensionAction(
                                    action.getId(),
                                    action.getTitle(),
                                    action.getTitle(),
                                    action.getTitle(),
                                    action.getHoverCardState());
                        });

        when(mExtensionsToolbarBridge.getAllActionIds())
                .thenReturn(new String[] {ACTION1_ID, ACTION2_ID, ACTION3_ID});
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
                        mRecyclerViewDelegate,
                        mExtensionsToolbarBridge,
                        /* contextMenuPopulatorFactory= */ null,
                        /* selectionDropdownMenuDelegate= */ null,
                        mTabModelSelector) {
                    @Override
                    Bitmap getIconForAction(String actionId, WebContents webContents) {
                        ActionData action = mActions.get(actionId);
                        assert action != null;

                        return action.getIcon();
                    }
                };

        mMediator.fitActionsWithinWidth(1000);
        verify(mExtensionsToolbarBridge).setActionListDelegate(mBridgeDelegateCaptor.capture());

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

        mActions.put(
                ACTION1_ID,
                new ActionData(
                        ACTION1_ID,
                        "new title of action 1",
                        ICON_CYAN,
                        new HoverCardState(
                                SiteAccess.ALL_EXTENSIONS_ALLOWED, "", "", AdminPolicy.NONE, "")));
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
        int itemWidth = context.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);

        // Test ample width.
        mMediator.fitActionsWithinWidth(itemWidth * 5);
        assertEquals(2, mModels.size());

        // Test width for 2 items.
        mMediator.fitActionsWithinWidth(itemWidth * 2);
        assertEquals(2, mModels.size());

        // Test width for more than 1 item but less than 2 items.
        mMediator.fitActionsWithinWidth(itemWidth + itemWidth / 2);
        assertEquals(1, mModels.size());

        // Test width for exactly 1 item.
        mMediator.fitActionsWithinWidth(itemWidth);
        assertEquals(1, mModels.size());

        // Test width insufficient for 1 item.
        mMediator.fitActionsWithinWidth(itemWidth - 1);

        // There should be 0 items.
        assertEquals(0, mModels.size());
    }

    @Test
    public void testPopOutAction_Unpinned_Popup() {
        // Action 3 is initially not in the model (unpinned).
        assertEquals(2, mModels.size());

        // Trigger a popup for Action 3 via the bridge delegate.
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION3_ID, 123L);
        mMediator.reconcileActionItems();

        // Action 3 should now be present in the models (popped out).
        assertEquals("Action 3 should be added to the list", 3, mModels.size());
        assertItemAt(2, ACTION3_ID, "title of action 3", ICON_GREEN);
    }

    @Test
    public void testPopOutAction_Unpinned_ContextMenu() {
        // Action 3 is initially not in the model.
        assertEquals(2, mModels.size());

        // Trigger a context menu for Action 3.
        mMediator.requestShowContextMenu(ACTION3_ID);
        mMediator.reconcileActionItems();

        // Action 3 should be popped out (added to the list).
        assertEquals("Action 3 should be temporarily added", 3, mModels.size());
        assertItemAt(2, ACTION3_ID, "title of action 3", ICON_GREEN);
    }

    @Test
    public void testPopOutAction_HiddenPinned_Popup() {
        // Get the button width.
        int buttonWidth =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);

        // Constrain width so only 1 action fits (we have 2 pinned actions).
        mMediator.fitActionsWithinWidth(buttonWidth);

        // Verify initial state: Only Action 1 is visible. Action 2 is hidden.
        assertEquals(1, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);

        // Execute: Trigger a popup for Action 2 (pinned but hidden).
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION2_ID, 123L);
        mMediator.reconcileActionItems();

        // Verify: Action 2 is temporarily added to the list (popped out).
        assertEquals("Action 2 should be added to the list", 2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);
    }

    @Test
    public void testUndoPopout() {
        // Action 3 is unpinned and not in the list.
        assertEquals(2, mModels.size());

        // Trigger popup for Action 3 via the bridge delegate.
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION3_ID, 123L);
        mMediator.reconcileActionItems();

        // The action is popped out (added to the list).
        assertEquals("Action 3 should be added to the list", 3, mModels.size());
        assertItemAt(2, ACTION3_ID, "title of action 3", ICON_GREEN);

        // Manually call {@code undoPopout()}.
        mMediator.undoPopout();
        mMediator.reconcileActionItems();

        // The action is removed from the list.
        assertEquals("Action 3 should be removed", 2, mModels.size());
        assertItemAt(0, ACTION1_ID, "title of action 1", ICON_RED);
        assertItemAt(1, ACTION2_ID, "title of action 2", ICON_BLUE);
    }

    @Test
    public void testPopOutAction_WidthReservation() {
        int buttonWidth =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_button_width);

        // Initially, no width is reserved because nothing is popped out.
        int reservedWidth = mMediator.setCanShowPoppedOutAction(1000);
        assertEquals("Should return 0 when no action is popped out", 0, reservedWidth);

        // Trigger a popup for an unpinned action (Action 3).
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION3_ID, 123L);
        mMediator.reconcileActionItems();

        // Now it should reserve the width of one button.
        reservedWidth = mMediator.setCanShowPoppedOutAction(1000);
        assertEquals(
                "Should return button width when an action is popped out",
                buttonWidth,
                reservedWidth);
    }

    @Test
    public void testPendingPopup_DestroyedOnCancellation() {
        // Trigger a popup.
        long nativeHostPtr = 123L;
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION1_ID, nativeHostPtr);

        // Verify the native contents were created.
        verify(mPopupContentsJniMock).create(nativeHostPtr);

        // Simulate a cancellation by opening a context menu for another action.
        mBridgeDelegateCaptor.getValue().showContextMenu(ACTION2_ID);

        // The pending popup contents must be destroyed to prevent memory leaks.
        verify(mPopupContentsMock).destroy();
    }

    @Test
    public void testPendingPopup_DestroyedOnMediatorTeardown() {
        // Trigger a popup to enter the PopupPending state.
        long nativeHostPtr = 123L;
        mBridgeDelegateCaptor.getValue().triggerPopup(ACTION1_ID, nativeHostPtr);

        // Destroy the mediator before the UI animation finishes.
        mMediator.destroy();

        // The pending popup contents must be destroyed during teardown.
        verify(mPopupContentsMock).destroy();
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
        assertEquals(title, item.model.get(ExtensionActionButtonProperties.ACCESSIBLE_NAME));
        assertTrue(icon.sameAs(item.model.get(ExtensionActionButtonProperties.ICON)));
    }
}

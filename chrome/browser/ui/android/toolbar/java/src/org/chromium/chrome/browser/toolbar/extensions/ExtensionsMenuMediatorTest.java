// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link ExtensionsMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuMediatorTest {
    // Constants identifying elements used in the test environment.
    private static final int TAB_ID = 111;
    private static final long ACTION_CONTEXT_MENU_BRIDGE_POINTER = 10000L;
    private static final long EXTENSIONS_MENU_BRIDGE_POINTER = 10001L;
    private static final long BROWSER_WINDOW_POINTER = 1000L;
    private static final Bitmap ICON_RED = ExtensionTestUtils.createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = ExtensionTestUtils.createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = ExtensionTestUtils.createSimpleIcon(Color.GREEN);
    private static final int ICON_MORE = R.drawable.ic_more_vert;
    private static final int ICON_KEEP = R.drawable.ic_keep_24dp;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    @Rule public final FakeExtensionUiBackendRule mUiBackendRule = new FakeExtensionUiBackendRule();

    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private ExtensionActionContextMenuBridge.Native mActionContextMenuBridgeJniMock;
    @Mock private ExtensionsMenuBridge.Natives mExtensionsMenuBridgeJniMock;
    @Mock private MenuModelBridge mActionContextMenuModelBridge;
    @Mock private PropertyModel mMenuPropertyModel;
    @Mock private Runnable mOnReadyRunnable;

    @Captor private ArgumentCaptor<ExtensionsMenuBridge> mBridgeCaptor;
    @Captor private ArgumentCaptor<ListMenuHost.PopupMenuShownListener> mPopupListenerCaptor;

    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    // This is the reference to the list that the mediator holds and modifies.
    // We hold it here to verify that the mediator correctly updates the model.
    private ModelList mActionModels;

    private ExtensionsMenuMediator mMenuMediator;

    private ExtensionsMenuTypes.SiteSettingsState mSiteSettingsState;

    @Before
    public void setUp() {
        // Mock AndroidChromeTask.
        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);

        // Mock {@link ExtensionActionContextMenuBridge}.
        ExtensionActionContextMenuBridgeJni.setInstanceForTesting(mActionContextMenuBridgeJniMock);
        when(mActionContextMenuBridgeJniMock.init(anyLong(), any(), any(), anyInt()))
                .thenReturn(ACTION_CONTEXT_MENU_BRIDGE_POINTER);
        when(mActionContextMenuBridgeJniMock.getMenuModelBridge(anyLong()))
                .thenReturn(mActionContextMenuModelBridge);
        when(mActionContextMenuModelBridge.populateModelList()).thenReturn(new ModelList());

        // Mock {@link ExtensionsMenuBridge}.
        ExtensionsMenuBridgeJni.setInstanceForTesting(mExtensionsMenuBridgeJniMock);

        // Mock site settings state.
        mSiteSettingsState = createSiteSettingsState("label", true);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(mSiteSettingsState);
        when(mExtensionsMenuBridgeJniMock.init(any(), anyLong()))
                .thenReturn(EXTENSIONS_MENU_BRIDGE_POINTER);

        // Set the current tab.
        MockTab tab = new MockTab(TAB_ID, mProfile);
        tab.setWebContentsOverrideForTesting(mWebContents);
        mCurrentTabSupplier.set(tab);

        // Create the extensions menu mediator to be tested.
        mActionModels = new ModelList();
        mMenuMediator =
                new ExtensionsMenuMediator(
                        ApplicationProvider.getApplicationContext(),
                        mTask,
                        mProfile,
                        mCurrentTabSupplier,
                        mActionModels,
                        mMenuPropertyModel,
                        null,
                        mOnReadyRunnable);

        // Capture the bridge instance created inside the constructor
        verify(mExtensionsMenuBridgeJniMock).init(mBridgeCaptor.capture(), anyLong());
    }

    @After
    public void tearDown() {
        mMenuMediator.destroy();
    }

    @Test
    public void testOnReady_ZeroState() {
        // Mock the bridge to return an empty list (no extensions).
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(new ArrayList<>());

        // Simulate the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();

        // Verify action models remain empty, property model is updated to show zero state and the
        // onReady runnable is called.
        assertTrue(mActionModels.isEmpty());
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, true);
        verify(mOnReadyRunnable).run();
    }

    @Test
    public void testOnReady_Actions() {
        // Mock the bridge to return menu entries.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ true));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Simulate the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();

        // Verify action models are populated, property model is updated to hide zero state and the
        // onReady runnable is called.
        assertEquals(2, mActionModels.size());
        assertItemAt(0, "Extension A", ICON_RED, ICON_KEEP);
        assertItemAt(1, "Extension B", ICON_BLUE, ICON_MORE);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
        verify(mOnReadyRunnable).run();
    }

    @Test
    public void testOnReady_AlreadyPopulated() {
        // Setup a new mediator context where the bridge is already ready during construction.
        when(mExtensionsMenuBridgeJniMock.isReady(anyLong())).thenReturn(true);

        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        mActionModels.clear();

        // Re-create mediator (simulating a fresh open where C++ is already ready).
        ExtensionsMenuMediator mediator =
                new ExtensionsMenuMediator(
                        ApplicationProvider.getApplicationContext(),
                        mTask,
                        mProfile,
                        mCurrentTabSupplier,
                        mActionModels,
                        mMenuPropertyModel,
                        null,
                        mOnReadyRunnable);

        // Verify it should have populated immediately without needing a callback.
        assertEquals(1, mActionModels.size());
        assertItemAt(0, "Extension A", ICON_RED, ICON_MORE);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);

        mediator.destroy();
    }

    @Test
    public void testOnActionIconUpdated() {
        // Initialize the action models.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", null, /* isPinned= */ false));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", null, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        mBridgeCaptor.getValue().onReady();

        // Verify icons are correct.
        assertItemAt(0, "Extension A", null, ICON_MORE);
        assertItemAt(1, "Extension B", null, ICON_MORE);

        // Simulate the native callback triggered when the icon for the first item is updated.
        int entryIndex = 0;
        when(mExtensionsMenuBridgeJniMock.getActionIcon(anyLong(), eq(entryIndex)))
                .thenReturn(ICON_GREEN);
        mBridgeCaptor.getValue().onActionIconUpdated(entryIndex);

        // Verify the firstaction model's icon has been updated to the green one.
        assertItemAt(0, "Extension A", ICON_GREEN, ICON_MORE);
        assertItemAt(1, "Extension B", null, ICON_MORE);
    }

    /**
     * Tests that clicking on the context menu button of an extension item opens the context menu.
     */
    @Test
    public void testContextClick_showMenu() {
        // Initialize the action models.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ true));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        mBridgeCaptor.getValue().onReady();

        // Click on the context menu button.
        ListItem item = mActionModels.get(0);
        View.OnClickListener contextMenuButtonListener =
                item.model.get(ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ON_CLICK);
        ListMenuButton mockContextMenuButton = createMockMenuButton();
        contextMenuButtonListener.onClick(mockContextMenuButton);

        // Verify the context menu is shown.
        verify(mActionContextMenuBridgeJniMock)
                .init(
                        eq(BROWSER_WINDOW_POINTER),
                        eq("id_a"),
                        eq(mWebContents),
                        eq(ContextMenuSource.MENU_ITEM));
        verify(mockContextMenuButton).showMenu();

        // Verify dismissal logic (required for cleanup).
        // Manually capture and fire the dismiss listener. This is required to
        // trigger bridge.destroy() and pass the test framework's leak check.
        verify(mockContextMenuButton).addPopupListener(mPopupListenerCaptor.capture());
        mPopupListenerCaptor.getValue().onPopupMenuDismissed();
        verify(mActionContextMenuBridgeJniMock).destroy(eq(ACTION_CONTEXT_MENU_BRIDGE_POINTER));
    }

    /** Helper to create a mock {@link ListMenuButton} with a mock {@link ListMenuHost}. */
    private ListMenuButton createMockMenuButton() {
        ListMenuHost mockListMenuHost = mock(ListMenuHost.class);
        when(mockListMenuHost.getHierarchicalMenuController())
                .thenReturn(mock(HierarchicalMenuController.class));

        ListMenuButton mockButton = mock(ListMenuButton.class);
        when(mockButton.getHost()).thenReturn(mockListMenuHost);
        return mockButton;
    }

    @Test
    public void testUpdateSiteSettingsToggle() {
        ExtensionsMenuTypes.SiteSettingsState siteSettingsState =
                createSiteSettingsState("test_label", true);

        mMenuMediator.updateSiteSettingsToggle(siteSettingsState);

        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "test_label");
    }

    @Test
    public void testSiteSettingsToggle_ClickCallsBridge() {
        ArgumentCaptor<android.widget.CompoundButton.OnCheckedChangeListener> captor =
                ArgumentCaptor.forClass(
                        android.widget.CompoundButton.OnCheckedChangeListener.class);
        verify(mMenuPropertyModel)
                .set(
                        eq(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER),
                        captor.capture());

        captor.getValue().onCheckedChanged(null, true);
        verify(mExtensionsMenuBridgeJniMock)
                .onSiteSettingsToggleChanged(EXTENSIONS_MENU_BRIDGE_POINTER, true);
    }

    /** Helper to assert that the item at the given index has the correct information. */
    private void assertItemAt(int index, String title, @Nullable Bitmap icon, int contextMenuIcon) {
        ListItem item = mActionModels.get(index);
        assertEquals(0, item.type);
        assertEquals(title, item.model.get(ExtensionsMenuItemProperties.TITLE));
        if (icon == null) {
            assertNull(item.model.get(ExtensionsMenuItemProperties.ICON));
        } else {
            assertTrue(icon.sameAs(item.model.get(ExtensionsMenuItemProperties.ICON)));
        }
        assertEquals(
                contextMenuIcon,
                item.model.get(ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ICON));
    }

    private ExtensionsMenuTypes.SiteSettingsState createSiteSettingsState(
            String label, boolean isOn) {
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        "toggle_text",
                        "accessible_name",
                        "tooltip",
                        isOn,
                        /* icon= */ null);
        return new ExtensionsMenuTypes.SiteSettingsState(
                label, /* hasTooltip= */ false, toggleState);
    }
}

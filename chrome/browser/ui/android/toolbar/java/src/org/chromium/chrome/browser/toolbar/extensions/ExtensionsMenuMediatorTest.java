// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.widget.CompoundButton;

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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

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
    @Mock private PropertyModel mSitePermissionsPropertyModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Runnable mOnDismissMenu;
    @Mock private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    @Mock private Runnable mOnReadyRunnable;

    @Captor private ArgumentCaptor<ExtensionsMenuBridge> mBridgeCaptor;
    @Captor private ArgumentCaptor<ListMenuHost.PopupMenuShownListener> mPopupListenerCaptor;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    @Captor
    private ArgumentCaptor<List<ExtensionsMenuTypes.HostAccessRequest>> mHostAccessRequestsCaptor;

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
        mSiteSettingsState =
                createSiteSettingsState(
                        "label",
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* isOn= */ false,
                        /* hasTooltip= */ false);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(mSiteSettingsState);
        when(mExtensionsMenuBridgeJniMock.init(any(), anyLong(), anyLong()))
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
                        mTabCreator,
                        mExtensionsToolbarBridge,
                        mActionModels,
                        mMenuPropertyModel,
                        mSitePermissionsPropertyModel,
                        mOnDismissMenu,
                        mOnReadyRunnable);

        // Capture the bridge instance created inside the constructor
        verify(mExtensionsMenuBridgeJniMock).init(mBridgeCaptor.capture(), anyLong(), anyLong());
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
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, false);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, false);
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
        assertItemAt(0, "Extension A", ICON_RED);
        assertItemAt(1, "Extension B", ICON_BLUE);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, true);
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
                        mTabCreator,
                        mExtensionsToolbarBridge,
                        mActionModels,
                        mMenuPropertyModel,
                        mSitePermissionsPropertyModel,
                        mOnDismissMenu,
                        mOnReadyRunnable);

        // Verify it should have populated immediately without needing a callback.
        assertEquals(1, mActionModels.size());
        assertItemAt(0, "Extension A", ICON_RED);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, true);

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
        assertItemAt(0, "Extension A", null);
        assertItemAt(1, "Extension B", null);

        // Simulate the native callback triggered when the icon for the first item is updated.
        int entryIndex = 0;
        when(mExtensionsMenuBridgeJniMock.getActionIcon(anyLong(), eq(entryIndex)))
                .thenReturn(ICON_GREEN);
        mBridgeCaptor.getValue().onActionIconUpdated(entryIndex);

        // Verify the firstaction model's icon has been updated to the green one.
        assertItemAt(0, "Extension A", ICON_GREEN);
        assertItemAt(1, "Extension B", null);
    }

    /**
     * Tests that updating an extension icon correctly updates its corresponding site permissions
     * page, if it's currently opened.
     */
    @Test
    public void testOnActionIconUpdated_SitePermissionsPage() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        // Add two extensions.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Mock being on the site permissions page for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Open extensions menu and go to Extension's A site permissions page.
        mBridgeCaptor.getValue().onReady();
        ListItem itemA = mActionModels.get(0);
        View.OnClickListener listener =
                itemA.model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK);
        listener.onClick(null);

        // Verify site permissions page has ICON_RED.
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_ICON, extensionIcon);

        // Mock being on the site permissions page for "id_a".
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        when(mSitePermissionsPropertyModel.get(SitePermissionsPageProperties.EXTENSION_ID))
                .thenReturn("id_a");

        // Update Extension A icon.
        int entryIndexA = 0;
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(entryIndexA)))
                .thenReturn(entries.get(entryIndexA));
        when(mExtensionsMenuBridgeJniMock.getActionIcon(anyLong(), eq(entryIndexA)))
                .thenReturn(ICON_GREEN);

        clearInvocations(mSitePermissionsPropertyModel);
        mBridgeCaptor.getValue().onActionIconUpdated(entryIndexA);

        // Verify icon in site permissions page is updated.
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_ICON, ICON_GREEN);

        // Update Extension B icon.
        int entryIndexB = 1;
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(entryIndexB)))
                .thenReturn(entries.get(entryIndexB));
        when(mExtensionsMenuBridgeJniMock.getActionIcon(anyLong(), eq(entryIndexB)))
                .thenReturn(ICON_BLUE);

        clearInvocations(mSitePermissionsPropertyModel);
        mBridgeCaptor.getValue().onActionIconUpdated(entryIndexB);

        // Verify icon in site permissions page is unchanged.
        verify(mSitePermissionsPropertyModel, never())
                .set(eq(SitePermissionsPageProperties.EXTENSION_ICON), any());
    }

    /** Tests that adding an extension action to the menu correctly updates the action models. */
    @Test
    public void testOnActionAdded() {
        // Initialize with one item.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();
        clearInvocations(mMenuPropertyModel);

        assertEquals(1, mActionModels.size());

        // Mock the new entry to be added.
        ExtensionsMenuTypes.MenuEntryState newEntry =
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(1))).thenReturn(newEntry);

        // Simulate the native callback triggered when a new item is added at the end.
        mBridgeCaptor.getValue().onActionAdded(1);

        // Verify that the new item is added at the correct index.
        assertEquals(2, mActionModels.size());
        assertItemAt(0, "Extension A", ICON_RED);
        assertItemAt(1, "Extension B", ICON_BLUE);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
    }

    /**
     * Tests that adding an extension action to an empty menu correctly updates the action models.
     */
    @Test
    public void testOnActionAdded_ZeroState() {
        // Initialize with no items.
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(new ArrayList<>());

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();
        clearInvocations(mMenuPropertyModel);

        assertEquals(0, mActionModels.size());

        // Mock the new entry to be added.
        ExtensionsMenuTypes.MenuEntryState newEntry =
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(newEntry);

        // Simulate the native callback triggered when an item is added to an empty menu.
        mBridgeCaptor.getValue().onActionAdded(0);

        // Verify that the new item is added and zero state is hidden.
        assertEquals(1, mActionModels.size());
        assertItemAt(0, "Extension A", ICON_RED);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, true);
    }

    /**
     * Tests that removing an extension action from the menu correctly updates the action models.
     */
    @Test
    public void testOnActionRemoved() {
        // Initialize the action models.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();
        clearInvocations(mMenuPropertyModel);

        assertEquals(2, mActionModels.size());

        // Simulate the native callback triggered when the first item is removed.
        mBridgeCaptor.getValue().onActionRemoved(0);

        // Verify that the first item is removed and the second item shifted to index 0.
        assertEquals(1, mActionModels.size());
        assertItemAt(0, "Extension B", ICON_BLUE);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, false);
    }

    /** Tests that removing the last extension action from the menu shows the zero state. */
    @Test
    public void testOnActionRemoved_ZeroState() {
        // Initialize with one item.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();
        clearInvocations(mMenuPropertyModel);

        assertEquals(1, mActionModels.size());

        // Simulate removal of the only action.
        mBridgeCaptor.getValue().onActionRemoved(0);

        // Verify zero state is shown.
        assertEquals(0, mActionModels.size());
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.IS_ZERO_STATE, true);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, false);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_VISIBLE, false);
    }

    /**
     * Tests that updating an extension action correctly updates its corresponding action model in
     * the menu, updating its order if necessary.
     */
    @Test
    public void testOnActionUpdated() {
        // Initialize the action models with two items.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();

        assertEquals(2, mActionModels.size());

        // Simulate the native callback for Extension A updated, maintaining its current index.
        ExtensionsMenuTypes.MenuEntryState updatedEntryA =
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A Updated", ICON_RED, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntryA);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify "Extension A" is updated at its current index.
        assertEquals(2, mActionModels.size());
        assertItemAt(0, "Extension A Updated", ICON_RED);
        assertItemAt(1, "Extension B", ICON_BLUE);

        // Simulate the native callback for Extension A updated, moving to a new index.
        ExtensionsMenuTypes.MenuEntryState updatedEntryB =
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B Updated", ICON_BLUE, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntryB);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify "Extension B" moved to index 0 and "Extension A" moved to index 1.
        assertEquals(2, mActionModels.size());
        assertItemAt(0, "Extension B Updated", ICON_BLUE);
        assertItemAt(1, "Extension A Updated", ICON_RED);
    }

    /**
     * Tests that updating an extension action correctly updates its corresponding site permissions
     * page, if it's currently opened.
     */
    @Test
    public void testOnActionUpdated_SitePermissionsPage() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        // Add two extensions.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B", ICON_BLUE, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Mock the site permissions page info for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Open extensions menu and go to Extension's A site permissions page.
        mBridgeCaptor.getValue().onReady();
        ListItem itemA = mActionModels.get(0);
        View.OnClickListener listener =
                itemA.model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK);
        listener.onClick(null);

        // Verify site permissions page has 'Extension A'.
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_NAME, extensionName);

        // Mock being on the site permissions page for "id_a".
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        when(mSitePermissionsPropertyModel.get(SitePermissionsPageProperties.EXTENSION_ID))
                .thenReturn("id_a");

        // Update Extension A name.
        ExtensionsMenuTypes.MenuEntryState updatedEntryA =
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", "Extension A Updated", ICON_RED, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntryA);
        ExtensionsMenuTypes.ExtensionSitePermissionsState updatedSitePermissionsStateA =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        "Extension A Updated", ICON_RED);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(updatedSitePermissionsStateA);

        clearInvocations(mSitePermissionsPropertyModel);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify name in site permissions page is updated.
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_NAME, "Extension A Updated");

        // Update Extension B name.
        ExtensionsMenuTypes.MenuEntryState updatedEntryB =
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_b", "Extension B Updated", ICON_BLUE, /* isPinned= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(1))).thenReturn(updatedEntryB);

        clearInvocations(mSitePermissionsPropertyModel);
        mBridgeCaptor.getValue().onActionUpdated(1);

        // Verify name in site permissions page is unchanged.
        verify(mSitePermissionsPropertyModel, never())
                .set(eq(SitePermissionsPageProperties.EXTENSION_NAME), any());
    }

    /**
     * Tests that clicking on the context menu button of an extension item opens the context menu.
     */
    @Test
    public void testContextMenuButton_showMenuOnClick() {
        // Initialize the action models.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createSimpleMenuEntry(
                        "id_a", "Extension A", ICON_RED, /* isPinned= */ false));
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

        // Verify context menu button is no longer selected after dismissal.
        verify(mActionContextMenuBridgeJniMock).destroy(eq(ACTION_CONTEXT_MENU_BRIDGE_POINTER));
    }

    /** Tests that the context menu button's accessible name is correctly updated in the model. */
    @Test
    public void testMenuItemContextMenuButtonAccessibleName() {
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        ExtensionsMenuTypes.ControlState actionButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        "Extension A",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        ICON_RED);
        ExtensionsMenuTypes.ControlState contextMenuButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "More options for Extension A",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);

        // updateMenuItem() accesses fields on siteAccessToggle and sitePermissionsButton, so
        // they cannot be null. We use a placeholder state since they are not relevant to this test.
        ExtensionsMenuTypes.ControlState placeholderState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        entries.add(
                new ExtensionsMenuTypes.MenuEntryState(
                        "id_a",
                        actionButton,
                        contextMenuButton,
                        placeholderState,
                        placeholderState,
                        /* isEnterprise= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        mBridgeCaptor.getValue().onReady();

        PropertyModel model = mActionModels.get(0).model;
        assertEquals(
                "More options for Extension A",
                model.get(ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ACCESSIBLE_NAME));
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

    /**
     * Tests that site settings toggle is correctly updated based on the state provided by the
     * native side.
     */
    @Test
    public void testUpdateSiteSettingsToggle() {
        // Verify toggle properties when it should be visible and on.
        ExtensionsMenuTypes.SiteSettingsState siteSettingsStateOn =
                createSiteSettingsState(
                        "test_label",
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* isOn= */ true,
                        /* hasTooltip= */ false);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(siteSettingsStateOn);
        mMenuMediator.updateSiteSettingsToggle();

        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "test_label");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_TOOLTIP, "tooltip");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_INFO_ICON_VISIBLE, false);

        clearInvocations(mMenuPropertyModel);

        // Verify toggle properties when it should be visible and off.
        ExtensionsMenuTypes.SiteSettingsState siteSettingsStateOff =
                createSiteSettingsState(
                        "test_label_2",
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* isOn= */ false,
                        /* hasTooltip= */ false);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(siteSettingsStateOff);
        mMenuMediator.updateSiteSettingsToggle();

        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, true);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, false);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "test_label_2");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_TOOLTIP, "tooltip");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_INFO_ICON_VISIBLE, false);

        clearInvocations(mMenuPropertyModel);

        // Verify toggle properties when it should be hidden (doesn't matter if toggle is on/off).
        ExtensionsMenuTypes.SiteSettingsState siteSettingsStateHidden =
                createSiteSettingsState(
                        "test_label_3",
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* isOn= */ false,
                        /* hasTooltip= */ false);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong()))
                .thenReturn(siteSettingsStateHidden);
        mMenuMediator.updateSiteSettingsToggle();

        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, false);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, false);
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "test_label_3");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_TOOLTIP, "tooltip");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_INFO_ICON_VISIBLE, false);
    }

    @Test
    public void testUpdateSiteSettingsToggle_WithTooltip() {
        ExtensionsMenuTypes.SiteSettingsState siteSettingsState =
                createSiteSettingsState(
                        "test_label",
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* isOn= */ true,
                        /* hasTooltip= */ true);
        when(mExtensionsMenuBridgeJniMock.getSiteSettings(anyLong())).thenReturn(siteSettingsState);
        mMenuMediator.updateSiteSettingsToggle();

        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_CONTAINER_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED, true);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.SITE_SETTINGS_LABEL, "test_label");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_TOOLTIP, "tooltip");
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.SITE_SETTINGS_INFO_ICON_VISIBLE, true);
    }

    @Test
    public void testSiteSettingsToggle_ClickCallsBridge() {
        mMenuMediator.onSiteSettingsToggleChanged(true);
        verify(mExtensionsMenuBridgeJniMock)
                .onSiteSettingsToggleChanged(EXTENSIONS_MENU_BRIDGE_POINTER, true);
    }

    @Test
    public void testOnHostAccessRequestAdded_SectionVisible() {
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        List<ExtensionsMenuTypes.HostAccessRequest> requests = new ArrayList<>();
        requests.add(new ExtensionsMenuTypes.HostAccessRequest("id1", "name1", null));
        when(mExtensionsMenuBridgeJniMock.getHostAccessRequests(anyLong())).thenReturn(requests);

        mMenuMediator.onHostAccessRequestAdded("id1");

        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, requests);
    }

    @Test
    public void testOnHostAccessRequestAdded_SectionNotVisible() {
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.NONE);

        mMenuMediator.onHostAccessRequestAdded("id1");

        verify(mMenuPropertyModel, never())
                .set(eq(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS), any());
    }

    @Test
    public void testOnHostAccessRequestUpdated_SectionVisible() {
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        List<ExtensionsMenuTypes.HostAccessRequest> requests = new ArrayList<>();
        requests.add(new ExtensionsMenuTypes.HostAccessRequest("id1", "name1", null));
        when(mExtensionsMenuBridgeJniMock.getHostAccessRequests(anyLong())).thenReturn(requests);

        mMenuMediator.onHostAccessRequestUpdated("id1");

        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, requests);
    }

    @Test
    public void testOnHostAccessRequestRemoved_SectionVisible() {
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        List<ExtensionsMenuTypes.HostAccessRequest> requests = new ArrayList<>();
        when(mExtensionsMenuBridgeJniMock.getHostAccessRequests(anyLong())).thenReturn(requests);

        mMenuMediator.onHostAccessRequestRemoved("id1");

        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, requests);
    }

    @Test
    public void testOnHostAccessRequestsCleared_SectionVisible() {
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        List<ExtensionsMenuTypes.HostAccessRequest> requests = new ArrayList<>();
        when(mExtensionsMenuBridgeJniMock.getHostAccessRequests(anyLong())).thenReturn(requests);

        mMenuMediator.onHostAccessRequestsCleared();

        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, requests);
    }

    @Test
    public void testOnShowHostAccessRequestsInToolbarChanged_SitePermissionsPage() {
        // Mock being on the site permissions page for "id_a".
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        when(mSitePermissionsPropertyModel.get(SitePermissionsPageProperties.EXTENSION_ID))
                .thenReturn("id1");

        // Mock the site permissions state returned by bridge.
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState("Extension A", null);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id1")))
                .thenReturn(sitePermissionsState);

        // Call the method.
        mMenuMediator.onShowHostAccessRequestsInToolbarChanged("id1");

        // Verify that updateSitePermissionsPage was called (by verifying side effects, e.g. setting
        // name).
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_NAME, "Extension A");
    }

    @Test
    public void testOnShowHostAccessRequestsInToolbarChanged_WrongPage() {
        // Mock being on the main page.
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.MAIN);

        // Call the method.
        mMenuMediator.onShowHostAccessRequestsInToolbarChanged("id1");

        // Verify that getExtensionSitePermissionsState was never called.
        verify(mExtensionsMenuBridgeJniMock, never())
                .getExtensionSitePermissionsState(anyLong(), any());
    }

    @Test
    public void testOnShowHostAccessRequestsInToolbarChanged_WrongExtension() {
        // Mock being on the site permissions page for "id2".
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        when(mSitePermissionsPropertyModel.get(SitePermissionsPageProperties.EXTENSION_ID))
                .thenReturn("id2");

        // Call the method.
        mMenuMediator.onShowHostAccessRequestsInToolbarChanged("id1");

        // Verify that getExtensionSitePermissionsState was never called for "id1".
        verify(mExtensionsMenuBridgeJniMock, never())
                .getExtensionSitePermissionsState(anyLong(), eq("id1"));
    }

    @Test
    public void testOnReloadPageButtonClicked() {
        mMenuMediator.onReloadPageButtonClicked();
        verify(mExtensionsMenuBridgeJniMock)
                .onReloadPageButtonClicked(EXTENSIONS_MENU_BRIDGE_POINTER);
    }

    /** Tests that the menu item's site access toggle properties are correctly updated. */
    @Test
    public void testMenuItemSiteAccessToggle() {
        // Initialize an action with hidden toggle state.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ true,
                        /* icon= */ null);
        // Initialize the site permissions button with a random state. We won't update this state,
        // even though it should also be affected when site access toggle changes, as
        // this test focuses on the site access toggle.
        ExtensionsMenuTypes.ControlState sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        entries.add(
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();
        clearInvocations(mMenuPropertyModel);

        // Verify toggle is hidden for the menu item.
        PropertyModel model = mActionModels.get(0).model;
        assertEquals(1, mActionModels.size());
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS));

        // Update the menu item to have a visible toggle, with disabled and on state.
        toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.DISABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "Allowed on this site",
                        /* isOn= */ true,
                        /* icon= */ null);
        ExtensionsMenuTypes.MenuEntryState updatedEntry =
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntry);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify toggle is disabled and checked for the menu item
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.DISABLED,
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS));
        assertTrue(model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_CHECKED));
        assertEquals(
                "Allowed on this site",
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_TOOLTIP));

        // Update the item to have a visible toggle, with enabled and off state.
        toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "Not allowed on this site",
                        /* isOn= */ false,
                        /* icon= */ null);
        updatedEntry =
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntry);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify toggle is enabled and unchecked for the menu item.
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.ENABLED,
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS));
        assertFalse(model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_CHECKED));
        assertEquals(
                "Not allowed on this site",
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_TOOLTIP));
    }

    /** Tests that clicking the menu item's site access toggle calls the bridge. */
    @Test
    public void testMenuItemSiteAccessToggle_ClickCallsBridge() {
        // Initialize an action with a visible toggle state.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        ExtensionsMenuTypes.ControlState sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        entries.add(
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu by simulating the native callback triggering onReady.
        mBridgeCaptor.getValue().onReady();

        // Get the toggle listener for the menu item.
        PropertyModel model = mActionModels.get(0).model;
        CompoundButton.OnCheckedChangeListener listener =
                model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_ON_CLICK);

        // Simulate a toggle change.
        listener.onCheckedChanged(null, true);

        // Verify that the bridge's onExtensionToggleSelected was called with the correct extension
        // ID and value.
        verify(mExtensionsMenuBridgeJniMock)
                .onExtensionToggleSelected(EXTENSIONS_MENU_BRIDGE_POINTER, "id_a", true);
    }

    /** Tests that the menu item's site permission button properties are correctly updated. */
    @Test
    public void testMenuItemSitePermissionsButton() {
        // Initialize an action with hidden site permissions button.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        ExtensionsMenuTypes.ControlState sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "No access needed",
                        /* accessibleName= */ "No access needed",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        // Initialize the site access toggle with a random state. We won't update this state,
        // even though it should also be affected when site permission button changes, as
        // this test focuses on the site permissions button..
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ true,
                        /* icon= */ null);
        entries.add(
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu.
        mBridgeCaptor.getValue().onReady();

        // Verify site permissions button is hidden for the menu item.
        PropertyModel model = mActionModels.get(0).model;
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS));

        // Update the menu item to have a visible site permissions button, with disabled state.
        sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.DISABLED,
                        /* text= */ "Always on all sites",
                        /* accessibleName= */ "Always on all sites. Select to change site"
                                + " permissions",
                        /* tooltipText= */ "Change site permissions",
                        /* isOn= */ false,
                        /* icon= */ null);
        ExtensionsMenuTypes.MenuEntryState updatedEntry =
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntry);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify button is disabled and has correct text and accessible name.
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.DISABLED,
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS));
        assertEquals(
                "Always on all sites",
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TEXT));
        assertEquals(
                "Always on all sites. Select to change site permissions",
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME));
        assertEquals(
                "Change site permissions",
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TOOLTIP));

        // Update the item to have an enabled site permissions button.
        sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "Ask on every visit",
                        /* accessibleName= */ "Ask on every visit. Select to change site"
                                + " permissions",
                        /* tooltipText= */ "Change site permissions",
                        /* isOn= */ false,
                        /* icon= */ null);
        updatedEntry =
                ExtensionTestUtils.createMenuEntry(
                        "id_a",
                        "Extension A",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ false);
        when(mExtensionsMenuBridgeJniMock.getMenuEntry(anyLong(), eq(0))).thenReturn(updatedEntry);
        mBridgeCaptor.getValue().onActionUpdated(0);

        // Verify button is enabled and has updated text and accessible name.
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.ENABLED,
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS));
        assertEquals(
                "Ask on every visit",
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TEXT));
        assertEquals(
                "Ask on every visit. Select to change site permissions",
                model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME));
    }

    @Test
    public void testOnModelChanged_HostAccessRequestsSection() {
        // Mock that the main page is visible and the optional section is HOST_ACCESS_REQUESTS
        when(mExtensionsMenuBridgeJniMock.getOptionalSection(anyLong()))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        List<ExtensionsMenuTypes.HostAccessRequest> requests = new ArrayList<>();
        requests.add(new ExtensionsMenuTypes.HostAccessRequest("id1", "name1", null));
        when(mExtensionsMenuBridgeJniMock.getHostAccessRequests(anyLong())).thenReturn(requests);

        mMenuMediator.onModelChanged();

        verify(mMenuPropertyModel)
                .set(
                        ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE,
                        ExtensionsMenuTypes.OptionalSectionType.HOST_ACCESS_REQUESTS);
        verify(mMenuPropertyModel).set(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS, requests);
    }

    @Test
    public void testOnModelChanged_ReloadSection() {
        // Mock that the main page is visible and the optional section is RELOAD_PAGE
        when(mExtensionsMenuBridgeJniMock.getOptionalSection(anyLong()))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.RELOAD_PAGE);

        mMenuMediator.onModelChanged();

        verify(mMenuPropertyModel)
                .set(
                        ExtensionsMenuProperties.OPTIONAL_SECTION_TYPE,
                        ExtensionsMenuTypes.OptionalSectionType.RELOAD_PAGE);

        // Verify that the HOST_ACCESS_REQUESTS property is cleared to an empty list
        verify(mMenuPropertyModel)
                .set(
                        eq(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS),
                        mHostAccessRequestsCaptor.capture());
        assertTrue(mHostAccessRequestsCaptor.getValue().isEmpty());
    }

    @Test
    public void testOnModelChanged_NoOptionalSection() {
        // Mock that the main page is visible and there is no optional section
        when(mExtensionsMenuBridgeJniMock.getOptionalSection(anyLong()))
                .thenReturn(ExtensionsMenuTypes.OptionalSectionType.NONE);

        mMenuMediator.onModelChanged();

        // Verify that the HOST_ACCESS_REQUESTS property is cleared to an empty list
        verify(mMenuPropertyModel)
                .set(
                        eq(ExtensionsMenuProperties.HOST_ACCESS_REQUESTS),
                        mHostAccessRequestsCaptor.capture());
        assertTrue(mHostAccessRequestsCaptor.getValue().isEmpty());
    }

    /**
     * Tests that clicking on an extension's site permissions button opens the site permissions page
     * for such extension, and clicking on the back button returns to the main page.
     */
    @Test
    public void testSitePermissionsButton_ClickNavigates() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        // Initialize an action with host permissions, whose menu entry has a site permissions
        // button.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Mock being on the site permissions page for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Open extensions menu.
        mBridgeCaptor.getValue().onReady();

        // Trigger the click listener for the site permissions button.
        PropertyModel itemModel = mActionModels.get(0).model;
        View.OnClickListener listener =
                itemModel.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK);
        listener.onClick(null);

        // Assert that the menu model and site permissions model are updated correctly.
        verify(mMenuPropertyModel)
                .set(
                        ExtensionsMenuProperties.CURRENT_PAGE,
                        ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_ID, "id_a");
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_NAME, extensionName);

        // Trigger the back button on the site permissions page.
        mMenuMediator.onBackButtonClicked();

        // Verify menu is back to the main page.
        verify(mMenuPropertyModel)
                .set(ExtensionsMenuProperties.CURRENT_PAGE, ExtensionsMenuProperties.Page.MAIN);
    }

    /**
     * Tests that clicking on the 'manage this extension' button on the site permissions page opens
     * the extensions management page for that specific extension.
     */
    @Test
    public void testSitePermissionsPage_OnManageThisExtensionClicked() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        // Add extension with host permissions.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Mock being on the site permissions page for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Open extensions menu and go to the site permissions page.
        mBridgeCaptor.getValue().onReady();
        PropertyModel itemModel = mActionModels.get(0).model;
        itemModel.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK).onClick(null);

        // Mock the state as if we navigated (since property models are mocks).
        when(mMenuPropertyModel.get(ExtensionsMenuProperties.CURRENT_PAGE))
                .thenReturn(ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        when(mSitePermissionsPropertyModel.get(SitePermissionsPageProperties.EXTENSION_ID))
                .thenReturn("id_a");

        // Click on 'manage this extension'.
        mMenuMediator.onManageThisExtensionClicked();

        // Verify dismiss runnable was called.
        verify(mOnDismissMenu).run();

        // Verify extension page was opened for the extension.
        verify(mTabCreator).createNewTab(mLoadUrlParamsCaptor.capture(), anyInt(), any());
        assertEquals(
                UrlConstants.CHROME_EXTENSIONS_URL + "?id=id_a",
                mLoadUrlParamsCaptor.getValue().getUrl());
    }

    /** Tests that clicking on the 'discover extensions' button opens the web store page. */
    @Test
    public void testOnDiscoverExtensionsClicked() {
        mMenuMediator.onDiscoverExtensionsClicked();

        verify(mOnDismissMenu).run();
        verify(mTabCreator).createNewTab(mLoadUrlParamsCaptor.capture(), anyInt(), any());
        assertEquals(UrlConstants.CHROME_WEBSTORE_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    /**
     * Tests that clicking on the 'manage extensions' button opens the extensions management page.
     */
    @Test
    public void testOnManageExtensionsClicked() {
        mMenuMediator.onManageExtensionsClicked();

        verify(mOnDismissMenu).run();
        verify(mTabCreator).createNewTab(mLoadUrlParamsCaptor.capture(), anyInt(), any());
        assertEquals(UrlConstants.CHROME_EXTENSIONS_URL, mLoadUrlParamsCaptor.getValue().getUrl());
    }

    /**
     * Tests that clicking on the 'show access requests' toggle on the site permissions page for an
     * extension notifies the bridge.
     */
    @Test
    public void testSitePermissionsPage_OnSitePermissionsButtonClicked() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        // Add extension with host permissions.
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open extensions menu.
        mBridgeCaptor.getValue().onReady();

        // Mock the site permissions page info for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Trigger the click listener for the site permissions button.
        PropertyModel itemModel = mActionModels.get(0).model;
        View.OnClickListener listener =
                itemModel.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK);
        listener.onClick(null);

        // Verify site permissions page is shown for the extension.
        verify(mMenuPropertyModel)
                .set(
                        ExtensionsMenuProperties.CURRENT_PAGE,
                        ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_ID, "id_a");

        // Verify toggle is checked.
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CHECKED, true);

        // Verify toggle click listener.
        ArgumentCaptor<CompoundButton.OnCheckedChangeListener> listenerCaptor =
                ArgumentCaptor.forClass(CompoundButton.OnCheckedChangeListener.class);
        verify(mSitePermissionsPropertyModel)
                .set(
                        eq(SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CLICK_LISTENER),
                        listenerCaptor.capture());

        listenerCaptor.getValue().onCheckedChanged(null, false);
        verify(mExtensionsMenuBridgeJniMock)
                .onShowRequestsTogglePressed(EXTENSIONS_MENU_BRIDGE_POINTER, "id_a", false);
    }

    /**
     * Tests that clicking on a site access option on the site permissions page for an extension
     * notifies the bridge with the correct option.
     */
    @Test
    public void testSitePermissionsPage_OnSiteAccessSelected() {
        String extensionName = "Extension A";
        Bitmap extensionIcon = ICON_RED;
        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(
                ExtensionTestUtils.createMenuEntryWithHostPermissions(
                        "id_a", extensionName, extensionIcon, /* isPinned= */ false));
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Mock the site permissions page info for "id_a".
        ExtensionsMenuTypes.ExtensionSitePermissionsState sitePermissionsState =
                ExtensionTestUtils.createExtensionSitePermissionsState(
                        extensionName, extensionIcon);
        when(mExtensionsMenuBridgeJniMock.getExtensionSitePermissionsState(anyLong(), eq("id_a")))
                .thenReturn(sitePermissionsState);

        // Open extensions menu and go to Extension's A site permissions page.
        mBridgeCaptor.getValue().onReady();
        ListItem itemA = mActionModels.get(0);
        View.OnClickListener listener =
                itemA.model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK);
        listener.onClick(null);

        // Verify site permissions page is shown for "id_a".
        verify(mMenuPropertyModel)
                .set(
                        ExtensionsMenuProperties.CURRENT_PAGE,
                        ExtensionsMenuProperties.Page.SITE_PERMISSIONS);
        verify(mSitePermissionsPropertyModel)
                .set(SitePermissionsPageProperties.EXTENSION_ID, "id_a");

        // Verify site access selected is 'on all sites'. Verify other options are enabled and off.
        verify(mSitePermissionsPropertyModel)
                .set(
                        SitePermissionsPageProperties.ON_CLICK_STATE,
                        sitePermissionsState.onClickOption);
        verify(mSitePermissionsPropertyModel)
                .set(
                        SitePermissionsPageProperties.ON_SITE_STATE,
                        sitePermissionsState.onSiteOption);
        verify(mSitePermissionsPropertyModel)
                .set(
                        SitePermissionsPageProperties.ON_ALL_SITES_STATE,
                        sitePermissionsState.onAllSitesOption);
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.ENABLED,
                sitePermissionsState.onClickOption.status);
        assertFalse(sitePermissionsState.onClickOption.isOn);
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.ENABLED,
                sitePermissionsState.onSiteOption.status);
        assertFalse(sitePermissionsState.onSiteOption.isOn);
        assertEquals(
                ExtensionsMenuTypes.ControlState.Status.ENABLED,
                sitePermissionsState.onAllSitesOption.status);
        assertTrue(sitePermissionsState.onAllSitesOption.isOn);

        // Select 'on site' option.
        ArgumentCaptor<Callback<Integer>> listenerCaptor = MockitoHelper.callbackCaptor();
        verify(mSitePermissionsPropertyModel)
                .set(
                        eq(SitePermissionsPageProperties.ON_SITE_ACCESS_SELECTED_LISTENER),
                        listenerCaptor.capture());
        listenerCaptor.getValue().onResult(ExtensionsMenuTypes.UserSiteAccess.ON_SITE);

        // Verify bridge is notified with the correct option.
        verify(mExtensionsMenuBridgeJniMock)
                .onSiteAccessSelected(
                        EXTENSIONS_MENU_BRIDGE_POINTER,
                        "id_a",
                        ExtensionsMenuTypes.UserSiteAccess.ON_SITE);
    }

    /**
     * Tests that an extension marked as enterprise (installed by policy) is correctly identified
     * and represented in the menu item property model.
     */
    @Test
    public void testEnterpriseExtension() {
        // Create a MenuEntryState with isEnterprise = true.
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ true,
                        /* icon= */ null);
        ExtensionsMenuTypes.ControlState sitePermissionsButtonState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        ExtensionsMenuTypes.MenuEntryState enterpriseEntry =
                ExtensionTestUtils.createMenuEntry(
                        "id_enterprise",
                        "Enterprise Extension",
                        ICON_RED,
                        /* isPinned= */ false,
                        toggleState,
                        sitePermissionsButtonState,
                        /* isEnterprise= */ true);

        List<ExtensionsMenuTypes.MenuEntryState> entries = new ArrayList<>();
        entries.add(enterpriseEntry);
        when(mExtensionsMenuBridgeJniMock.getMenuEntries(anyLong())).thenReturn(entries);

        // Open the menu.
        mBridgeCaptor.getValue().onReady();

        // Verify the PropertyModel reflects the enterprise state.
        PropertyModel model = mActionModels.get(0).model;
        assertTrue(
                "PropertyModel IS_ENTERPRISE should be true",
                model.get(ExtensionsMenuItemProperties.IS_ENTERPRISE));
    }

    /** Helper to assert that the item at the given index has the correct information. */
    private void assertItemAt(int index, String title, @Nullable Bitmap icon) {
        ListItem item = mActionModels.get(index);
        assertEquals(0, item.type);
        assertEquals(title, item.model.get(ExtensionsMenuItemProperties.TITLE));
        if (icon == null) {
            assertNull(item.model.get(ExtensionsMenuItemProperties.ICON));
        } else {
            assertTrue(icon.sameAs(item.model.get(ExtensionsMenuItemProperties.ICON)));
        }
    }

    private ExtensionsMenuTypes.SiteSettingsState createSiteSettingsState(
            String label,
            @ExtensionsMenuTypes.ControlState.Status int status,
            boolean isOn,
            boolean hasTooltip) {
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        status,
                        "toggle_text",
                        "accessible_name",
                        "tooltip",
                        isOn,
                        /* icon= */ null);
        return new ExtensionsMenuTypes.SiteSettingsState(label, hasTooltip, toggleState);
    }
}

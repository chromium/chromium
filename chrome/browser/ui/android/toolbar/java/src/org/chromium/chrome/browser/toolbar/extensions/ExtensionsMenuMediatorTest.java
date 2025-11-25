// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Looper;
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
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridgeJni;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ActionData;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.MenuModelBridge;
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
    private static final long ACTION_CONTEXT_MENU_BRIDGE_POINTER = 10000L;
    private static final long BROWSER_WINDOW_POINTER = 1000L;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = createSimpleIcon(Color.GREEN);
    private static final Bitmap ICON_CYAN = createSimpleIcon(Color.CYAN);
    private static final Bitmap ICON_MAGENTA = createSimpleIcon(Color.MAGENTA);
    private static final Bitmap ICON_YELLOW = createSimpleIcon(Color.YELLOW);
    private static final Bitmap ICON_WHITE = createSimpleIcon(Color.WHITE);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;
    @Mock private Runnable mDataReadyCallback;
    @Mock private Callback<Boolean> mOnExtensionsSupportedCallback;
    @Mock private WebContents mWebContents;
    @Mock private ExtensionActionContextMenuBridge.Native mActionContextMenuBridgeJniMock;
    @Mock private MenuModelBridge mMenuModelBridge;

    @Captor private ArgumentCaptor<ListMenuHost.PopupMenuShownListener> mPopupListenerCaptor;

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    @Rule public final FakeExtensionUiBackendRule mUiBackendRule = new FakeExtensionUiBackendRule();

    private final FakeExtensionActionsBridge mActionsBridge = mBridgeRule.getFakeBridge();

    private ProfileModel mProfileModel;
    private MockTab mTab1;
    private MockTab mTab2;
    private OneshotSupplierImpl<ChromeAndroidTask> mTaskSupplier;
    private ObservableSupplierImpl<@Nullable Profile> mProfileSupplier;
    private ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier;
    private ModelList mModels;
    private ExtensionsMenuMediator mMediator;

    @Before
    public void setUp() {
        mProfileModel = mActionsBridge.getOrCreateProfileModel(mProfile);
        mProfileModel.setInitialized(true);
        mProfileModel.putAction(
                "a", new ActionData.Builder().setTitle("title of a").setIcon(ICON_RED).build());
        mProfileModel.putAction(
                "b", new ActionData.Builder().setTitle("title of b").setIcon(ICON_GREEN).build());

        // Mock AndroidChromeTask.
        when(mTask.getOrCreateNativeBrowserWindowPtr()).thenReturn(BROWSER_WINDOW_POINTER);

        // Mock {@link ExtensionActionContextMenuBridge}.
        ExtensionActionContextMenuBridgeJni.setInstanceForTesting(mActionContextMenuBridgeJniMock);
        when(mActionContextMenuBridgeJniMock.init(anyLong(), any(), any(), anyInt()))
                .thenReturn(ACTION_CONTEXT_MENU_BRIDGE_POINTER);
        when(mActionContextMenuBridgeJniMock.getMenuModelBridge(anyLong()))
                .thenReturn(mMenuModelBridge);
        when(mMenuModelBridge.populateModelList()).thenReturn(new ModelList());

        // Initialize common objects.
        mTab1 = new MockTab(TAB1_ID, mProfile);
        mTab2 = new MockTab(TAB2_ID, mProfile);
        mTab1.setWebContentsOverrideForTesting(mWebContents);
        mTab2.setWebContentsOverrideForTesting(mWebContents);
        mTaskSupplier = new OneshotSupplierImpl<>();
        mTaskSupplier.set(mTask);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mCurrentTabSupplier = new ObservableSupplierImpl<>();
        mModels = new ModelList();

        mMediator =
                new ExtensionsMenuMediator(
                        ApplicationProvider.getApplicationContext(),
                        mTaskSupplier,
                        mProfileSupplier,
                        mCurrentTabSupplier,
                        mModels,
                        mDataReadyCallback,
                        mOnExtensionsSupportedCallback,
                        null);

        // Wait for the main thread to settle.
        shadowOf(Looper.getMainLooper()).idle();
    }

    @After
    public void tearDown() {
        mMediator.destroy();
    }

    @Test
    public void testExtensionsSupportedCallback() {
        Mockito.clearInvocations(mOnExtensionsSupportedCallback);
        mProfileSupplier.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mOnExtensionsSupportedCallback).onResult(true);

        mProfileSupplier.set(null);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mOnExtensionsSupportedCallback).onResult(false);

        mUiBackendRule.setEnabled(false);
        mProfileSupplier.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mOnExtensionsSupportedCallback).onResult(false);
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
    }

    @Test
    public void testUpdateModels_noTab() {
        // Set the profile only.
        mProfileSupplier.set(mProfile);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());
    }

    @Test
    public void testUpdateModels_tabChanged() {
        // Set up tab-dependent actions.
        mProfileModel.putAction(
                "a",
                (tabId) -> {
                    switch (tabId) {
                        case TAB1_ID:
                            return new ActionData.Builder()
                                    .setTitle("a for tab1")
                                    .setIcon(ICON_RED)
                                    .build();
                        case TAB2_ID:
                            return new ActionData.Builder()
                                    .setTitle("a for tab2")
                                    .setIcon(ICON_CYAN)
                                    .build();
                        default:
                            throw new RuntimeException("Unknown tab ID: " + tabId);
                    }
                });
        mProfileModel.putAction(
                "b",
                (tabId) -> {
                    switch (tabId) {
                        case TAB1_ID:
                            return new ActionData.Builder()
                                    .setTitle("b for tab1")
                                    .setIcon(ICON_GREEN)
                                    .build();
                        case TAB2_ID:
                            return new ActionData.Builder()
                                    .setTitle("b for tab2")
                                    .setIcon(ICON_MAGENTA)
                                    .build();
                        default:
                            throw new RuntimeException("Unknown tab ID: " + tabId);
                    }
                });

        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a for tab1", ICON_RED);
        assertItemAt(1, "b for tab1", ICON_GREEN);

        // Simulate changing the tab.
        mCurrentTabSupplier.set(mTab2);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a for tab2", ICON_CYAN);
        assertItemAt(1, "b for tab2", ICON_MAGENTA);
    }

    @Test
    public void testContextClick_showMenu() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        ListItem item = mModels.get(0);
        View.OnClickListener listener = item.model.get(ExtensionsMenuItemProperties.CLICK_LISTENER);

        // Stub helper calls on the mock button.
        ListMenuHost mockListMenuHost = mock(ListMenuHost.class);
        when(mockListMenuHost.getHierarchicalMenuController())
                .thenReturn(mock(HierarchicalMenuController.class));

        ListMenuButton mockButton = mock(ListMenuButton.class);
        when(mockButton.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mockButton.getHost()).thenReturn(mockListMenuHost);
        when(mockButton.getRootView())
                .thenReturn(new View(ApplicationProvider.getApplicationContext()));
        when(mockButton.getResources())
                .thenReturn(ApplicationProvider.getApplicationContext().getResources());

        listener.onClick(mockButton);

        verify(mActionContextMenuBridgeJniMock)
                .init(
                        eq(BROWSER_WINDOW_POINTER),
                        eq("a"),
                        eq(mWebContents),
                        eq(ContextMenuSource.MENU_ITEM));

        verify(mockButton).showMenu();

        // Manually capture and fire the dismiss listener. This is required to
        // trigger bridge.destroy() and pass the test framework's leak check.
        verify(mockButton).addPopupListener(mPopupListenerCaptor.capture());
        mPopupListenerCaptor.getValue().onPopupMenuDismissed();
        verify(mActionContextMenuBridgeJniMock).destroy(eq(ACTION_CONTEXT_MENU_BRIDGE_POINTER));
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ActionData;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for ExtensionActionsUpdateHelper. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ExtensionActionsUpdateHelperTest {
    private static final int TAB1_ID = 111;
    private static final int TAB2_ID = 222;
    private static final int ICON_CANVAS_WIDTH_DP = 12;
    private static final int ICON_CANVAS_HEIGHT_DP = 12;
    private static final float SCALE_FACTOR = 1.0f;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = createSimpleIcon(Color.GREEN);
    private static final Bitmap ICON_CYAN = createSimpleIcon(Color.CYAN);
    private static final Bitmap ICON_MAGENTA = createSimpleIcon(Color.MAGENTA);
    private static final Bitmap ICON_WHITE = createSimpleIcon(Color.WHITE);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ExtensionActionsUpdateHelper.ActionsUpdateDelegate mDelegate;
    @Mock private WebContents mWebContents;

    @Rule
    public final FakeExtensionActionsBridgeRule mFakeBridgeRule =
            new FakeExtensionActionsBridgeRule();

    private ProfileModel mProfileModel;
    private MockTab mTab1;
    private MockTab mTab2;
    private ObservableSupplierImpl<@Nullable Profile> mProfileSupplier;
    private ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier;
    private ModelList mModels;
    private ExtensionActionsUpdateHelper mHelper;

    @Before
    public void setUp() {
        mProfileModel = mFakeBridgeRule.getFakeBridge().getOrCreateProfileModel(mProfile);

        mTab1 = new MockTab(TAB1_ID, mProfile);
        mTab2 = new MockTab(TAB2_ID, mProfile);

        mProfileSupplier = new ObservableSupplierImpl<>();
        mCurrentTabSupplier = new ObservableSupplierImpl<>();
        mModels = new ModelList();

        // Provide good defaults for action queries via JNI.
        mProfileModel.setInitialized(true);
        mProfileModel.putAction(
                "a", new ActionData.Builder().setTitle("title of a").setIcon(ICON_RED).build());
        mProfileModel.putAction(
                "b", new ActionData.Builder().setTitle("title of b").setIcon(ICON_GREEN).build());

        when(mDelegate.createActionModel(any(), anyInt(), any()))
                .thenAnswer(
                        invocation -> {
                            ExtensionActionsBridge bridge = invocation.getArgument(0);
                            int tabId = invocation.getArgument(1);
                            String actionId = invocation.getArgument(2);
                            ExtensionAction action = bridge.getAction(actionId, tabId);
                            Bitmap icon =
                                    bridge.getActionIcon(
                                            actionId,
                                            tabId,
                                            mWebContents,
                                            ICON_CANVAS_WIDTH_DP,
                                            ICON_CANVAS_HEIGHT_DP,
                                            SCALE_FACTOR);
                            PropertyModel model =
                                    new PropertyModel.Builder(
                                                    ExtensionActionButtonProperties.ALL_KEYS)
                                            .with(
                                                    ExtensionActionButtonProperties.ID,
                                                    action.getId())
                                            .with(
                                                    ExtensionActionButtonProperties.TITLE,
                                                    action.getTitle())
                                            .with(ExtensionActionButtonProperties.ICON, icon)
                                            .build();
                            return new ListItem(ListItemType.EXTENSION_ACTION, model);
                        });

        mHelper =
                new ExtensionActionsUpdateHelper(
                        mModels, mProfileSupplier, mCurrentTabSupplier, mDelegate);
    }

    @Test
    public void updateActions() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);
    }

    @Test
    public void updateActions_noProfile() {
        // Set the tab only.
        mCurrentTabSupplier.set(mTab1);

        assertTrue(mModels.isEmpty());
    }

    @Test
    public void testUpdateModels_noTab() {
        // Set the profile only.
        mProfileSupplier.set(mProfile);

        assertTrue(mModels.isEmpty());
    }

    @Test
    public void updateActions_actionsInitializedLater() {
        // Actions are initially uninitialized.
        mProfileModel.setInitialized(false);

        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());

        // Notify that toolbar model has been initialized.
        mProfileModel.setInitialized(true);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);
    }

    @Test
    public void testUpdateModels_actionsAddedAndRemoved() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);

        // Update the actions.
        mProfileModel.putAction(
                "c", new ActionData.Builder().setTitle("title of c").setIcon(ICON_BLUE).build());
        mProfileModel.removeAction("a");

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "b", "title of b", ICON_GREEN);
        assertItemAt(1, "c", "title of c", ICON_BLUE);
    }

    @Test
    public void testUpdateModels_tabChanged() {
        // Set up per-tab variants.
        mProfileModel.putAction(
                "a",
                (tabId) -> {
                    switch (tabId) {
                        case TAB1_ID:
                            return new ActionData.Builder()
                                    .setTitle("title of a")
                                    .setIcon(ICON_RED)
                                    .build();
                        case TAB2_ID:
                            return new ActionData.Builder()
                                    .setTitle("another title of a")
                                    .setIcon(ICON_CYAN)
                                    .build();
                        default:
                            throw new RuntimeException();
                    }
                });
        mProfileModel.putAction(
                "b",
                (tabId) -> {
                    switch (tabId) {
                        case TAB1_ID:
                            return new ActionData.Builder()
                                    .setTitle("title of b")
                                    .setIcon(ICON_GREEN)
                                    .build();
                        case TAB2_ID:
                            return new ActionData.Builder()
                                    .setTitle("another title of b")
                                    .setIcon(ICON_MAGENTA)
                                    .build();
                        default:
                            throw new RuntimeException();
                    }
                });

        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);

        // Simulate changing the tab.
        mCurrentTabSupplier.set(mTab2);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "another title of a", ICON_CYAN);
        assertItemAt(1, "b", "another title of b", ICON_MAGENTA);
    }

    @Test
    public void testUpdateModels_iconUpdated() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);

        // Simulate changing the icon.
        mProfileModel.updateActionIcon(
                "a", new ActionData.Builder().setTitle("title of a").setIcon(ICON_WHITE).build());

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_WHITE);
        assertItemAt(1, "b", "title of b", ICON_GREEN);
    }

    @Test
    public void testUpdateModels_pinnedActionUpdated() {
        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);

        // Simulate changing the pinned actions.
        mProfileModel.putAction(
                "c", new ActionData.Builder().setTitle("title of c").setIcon(ICON_BLUE).build());

        // The model should have been updated.
        assertEquals(3, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);
        assertItemAt(2, "c", "title of c", ICON_BLUE);
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

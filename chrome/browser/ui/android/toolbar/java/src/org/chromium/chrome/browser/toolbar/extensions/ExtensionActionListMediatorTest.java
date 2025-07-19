// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
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
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ExtensionActionListMediatorTest {
    private static final int TAB1_ID = 111;
    private static final int TAB2_ID = 222;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);
    private static final Bitmap ICON_BLUE = createSimpleIcon(Color.BLUE);
    private static final Bitmap ICON_GREEN = createSimpleIcon(Color.GREEN);
    private static final Bitmap ICON_CYAN = createSimpleIcon(Color.CYAN);
    private static final Bitmap ICON_MAGENTA = createSimpleIcon(Color.MAGENTA);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;

    private FakeExtensionActionsBridge mFakeExtensionActionsBridge;
    private FakeExtensionActionsBridge.ProfileModel mProfileModel;
    private MockTab mTab1;
    private MockTab mTab2;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<Tab> mCurrentTabSupplier;
    private ModelList mModels;
    private ExtensionActionListMediator mMediator;

    @Before
    public void setUp() {
        mFakeExtensionActionsBridge = new FakeExtensionActionsBridge();
        mFakeExtensionActionsBridge.install();

        // Initialize common objects.
        mTab1 = new MockTab(TAB1_ID, mProfile);
        mTab2 = new MockTab(TAB2_ID, mProfile);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mCurrentTabSupplier = new ObservableSupplierImpl<>();
        mModels = new ModelList();
        mMediator =
                new ExtensionActionListMediator(
                        mContext, mWindowAndroid, mModels, mProfileSupplier, mCurrentTabSupplier);

        // Wait for the main thread to settle.
        shadowOf(Looper.getMainLooper()).idle();
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        mFakeExtensionActionsBridge.uninstall();
    }

    @Test
    public void testUpdateModels() {
        setUpProfileModel();

        // Set the profile and the tab.
        mProfileSupplier.set(mProfile);
        mCurrentTabSupplier.set(mTab1);

        // The model should have been updated.
        assertEquals(2, mModels.size());
        assertItemAt(0, "a", "title of a", ICON_RED);
        assertItemAt(1, "b", "title of b", ICON_GREEN);
    }

    @Test
    public void testUpdateModels_noProfile() {
        // Set the tab only.
        mCurrentTabSupplier.set(mTab1);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());
        verify(mProfile, never()).getNativeBrowserContextPointer();
    }

    @Test
    public void testUpdateModels_noTab() {
        setUpProfileModel();

        // Set the profile only.
        mProfileSupplier.set(mProfile);

        // The model should have been not updated.
        assertTrue(mModels.isEmpty());
    }

    @Test
    public void testUpdateModels_tabChanged() {
        setUpProfileModel();

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

    private void setUpProfileModel() {
        mProfileModel = mFakeExtensionActionsBridge.getOrCreateProfileModel(mProfile);
        mProfileModel.setInitialized(true);

        mProfileModel.putAction(
                "a",
                (tabId) -> {
                    if (tabId == TAB1_ID) {
                        return new FakeExtensionActionsBridge.ActionData.Builder()
                                .setTitle("title of a")
                                .setIcon(ICON_RED)
                                .build();
                    } else {
                        return new FakeExtensionActionsBridge.ActionData.Builder()
                                .setTitle("another title of a")
                                .setIcon(ICON_CYAN)
                                .build();
                    }
                });
        mProfileModel.putAction(
                "b",
                (tabId) -> {
                    if (tabId == TAB1_ID) {
                        return new FakeExtensionActionsBridge.ActionData.Builder()
                                .setTitle("title of b")
                                .setIcon(ICON_GREEN)
                                .build();
                    } else {
                        return new FakeExtensionActionsBridge.ActionData.Builder()
                                .setTitle("another title of b")
                                .setIcon(ICON_MAGENTA)
                                .build();
                    }
                });
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

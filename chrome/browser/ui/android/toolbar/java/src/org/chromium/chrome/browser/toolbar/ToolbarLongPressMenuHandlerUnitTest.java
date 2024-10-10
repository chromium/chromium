// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarApi26;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit tests for {@link ToolbarLongPressMenuHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
public final class ToolbarLongPressMenuHandlerUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private UrlBar mUrlBar;
    @Mock private ViewGroup mContentViewGroup;
    @Mock UiWidgetFactory mMockUiWidgetFactory;
    @Spy PopupWindow mSpyPopupWindow;

    private ToolbarLongPressMenuHandler mToolbarLongPressMenuHandler;

    private Activity mActivity;
    private ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(mActivity.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, false);

        UrlBar urlBar = new UrlBarApi26(mActivity, null);
        mUrlBar = spy(urlBar);

        mOmniboxFocusStateSupplier = new ObservableSupplierImpl<>();
        mOmniboxFocusStateSupplier.set(false);
        mToolbarLongPressMenuHandler =
                new ToolbarLongPressMenuHandler(mActivity, false, mOmniboxFocusStateSupplier);
        mUrlBar.setOnLongClickListener(mToolbarLongPressMenuHandler.getOnLongClickListener());
    }

    @After
    public void tearDown() throws Exception {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
        UiWidgetFactory.setInstance(null);
    }

    @Test
    @SmallTest
    @Config(qualifiers = "ldltr-sw600dp")
    public void testNoListenerOnTablet() {
        assertNull(mToolbarLongPressMenuHandler.getOnLongClickListener());
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    @Config(qualifiers = "sw400dp")
    public void testReturnListenerOnPhone() {
        assertNotNull(mToolbarLongPressMenuHandler.getOnLongClickListener());
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testDisplayLongpressMenu() {
        // Spy the popupwindow
        mSpyPopupWindow = spy(UiWidgetFactory.getInstance().createPopupWindow(mActivity));
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);

        // Making sure the popupwindow is big enough to display.
        doReturn(true).when(mUrlBar).isAttachedToWindow();
        doReturn(mContentViewGroup).when(mSpyPopupWindow).getContentView();
        doReturn(100).when(mContentViewGroup).getMeasuredWidth();
        doReturn(100).when(mContentViewGroup).getMeasuredHeight();
        doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        mToolbarLongPressMenuHandler.getOnLongClickListener().onLongClick(mUrlBar);
        verify(mSpyPopupWindow).showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testNoDisplayLongpressMenuWhenFocus() {
        mOmniboxFocusStateSupplier.set(true);
        mToolbarLongPressMenuHandler.getOnLongClickListener().onLongClick(mUrlBar);

        assertNull(mToolbarLongPressMenuHandler.getPopupWindowForTesting());
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testbuildMenuItemsWhenToolbarOnTop() {
        ModelList list = mToolbarLongPressMenuHandler.buildMenuItems();

        assertEquals(
                R.string.toolbar_move_to_the_bottom,
                list.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                ToolbarLongPressMenuHandler.MenuItemType.MOVE_ADDRESS_BAR_TO,
                list.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        assertEquals(
                R.string.toolbar_copy_link, list.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                ToolbarLongPressMenuHandler.MenuItemType.COPY_LINK,
                list.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testbuildMenuItemsWhenToolbarOnBottom() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false);
        ModelList list = mToolbarLongPressMenuHandler.buildMenuItems();

        assertEquals(
                R.string.toolbar_move_to_the_top,
                list.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                ToolbarLongPressMenuHandler.MenuItemType.MOVE_ADDRESS_BAR_TO,
                list.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        assertEquals(
                R.string.toolbar_copy_link, list.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                ToolbarLongPressMenuHandler.MenuItemType.COPY_LINK,
                list.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }
}

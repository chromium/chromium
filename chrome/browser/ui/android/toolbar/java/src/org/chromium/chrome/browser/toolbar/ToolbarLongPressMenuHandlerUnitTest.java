// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Rect;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarApi26;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.ClipboardImpl;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.UiWidgetFactory;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ToolbarLongPressMenuHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
public final class ToolbarLongPressMenuHandlerUnitTest {
    private static final int URLBAR_LEFT = 100;
    private static final int URLBAR_TOP = 20;
    private static final int URLBAR_RIGHT = 300;
    private static final int URLBAR_BOTTOM = 150;
    private static final int LONG_PRESS_MENU_WIDTH = 80;
    private static final int LONG_PRESS_MENU_HEIGHT = 30;
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private UrlBar mUrlBar;
    @Mock private ViewGroup mContentViewGroup;
    @Mock private BasicListMenu mBasicListMenu;
    @Mock private ViewRectProvider mViewRectProvider;
    @Mock UiWidgetFactory mMockUiWidgetFactory;
    @Mock Profile mProfile;
    @Mock Tracker mTracker;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Spy PopupWindow mSpyPopupWindow;

    private ToolbarLongPressMenuHandler mToolbarLongPressMenuHandler;
    private ObservableSupplierImpl mProfileSupplier;

    private Activity mActivity;
    private ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier;
    private SharedPreferencesManager mSharedPreferencesManager;
    private String mUrlString;
    private Configuration mConfiguration;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(mActivity.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, false);

        UrlBar urlBar = new UrlBarApi26(mActivity, null);
        mUrlBar = spy(urlBar);

        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mProfile);

        TrackerFactory.setTrackerForTests(mTracker);

        mOmniboxFocusStateSupplier = new ObservableSupplierImpl<>();
        mOmniboxFocusStateSupplier.set(false);
        mConfiguration = mActivity.getResources().getConfiguration();
        mConfiguration.screenWidthDp = 320;
        doReturn(mDisplayAndroid).when(mWindowAndroid).getDisplay();
        doReturn(1.0f).when(mDisplayAndroid).getDipScale();
        doReturn(true).when(mActivityLifecycleDispatcher).isNativeInitializationFinished();
        mToolbarLongPressMenuHandler =
                new ToolbarLongPressMenuHandler(
                        mActivity,
                        mProfileSupplier,
                        false,
                        mOmniboxFocusStateSupplier,
                        mActivityLifecycleDispatcher,
                        mWindowAndroid,
                        () -> mUrlString,
                        () -> mViewRectProvider);
        mUrlBar.setOnLongClickListener(mToolbarLongPressMenuHandler.getOnLongClickListener());

        verify(mActivityLifecycleDispatcher).register(mToolbarLongPressMenuHandler);

        doReturn(new Rect(URLBAR_LEFT, URLBAR_TOP, URLBAR_RIGHT, URLBAR_BOTTOM))
                .when(mViewRectProvider)
                .getRect();

        doReturn(new int[] {LONG_PRESS_MENU_WIDTH, LONG_PRESS_MENU_HEIGHT})
                .when(mBasicListMenu)
                .getMenuDimensions();

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @After
    public void tearDown() throws Exception {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
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

        verify(mTracker).notifyEvent(EventConstants.BOTTOM_TOOLBAR_MENU_TRIGGERED);

        assertNotNull(mToolbarLongPressMenuHandler.getPopupWindowForTesting());
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
        ModelList list = mToolbarLongPressMenuHandler.buildMenuItems(true);

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
        ModelList list = mToolbarLongPressMenuHandler.buildMenuItems(false);

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

    @Test
    @SmallTest
    public void testHandleMoveAddressBarTo() {
        mSharedPreferencesManager.writeBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);
        mToolbarLongPressMenuHandler.handleMenuClick(
                ToolbarLongPressMenuHandler.MenuItemType.MOVE_ADDRESS_BAR_TO);
        assertFalse(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true));
        mToolbarLongPressMenuHandler.handleMenuClick(
                ToolbarLongPressMenuHandler.MenuItemType.MOVE_ADDRESS_BAR_TO);
        assertTrue(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false));
    }

    @Test
    @SmallTest
    public void testHandleCopyLink() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);
        mUrlString = JUnitTestGURLs.URL_1.getSpec();

        mToolbarLongPressMenuHandler.handleMenuClick(
                ToolbarLongPressMenuHandler.MenuItemType.COPY_LINK);

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals("url", clipCaptor.getValue().getDescription().getLabel());
        assertEquals(mUrlString, clipCaptor.getValue().getItemAt(0).getText());
    }

    @Test
    @SmallTest
    public void testCalculateShowLocationOnTop_notRtl() {
        int[] location =
                mToolbarLongPressMenuHandler.calculateShowLocation(true, false, mBasicListMenu);
        assertEquals(
                URLBAR_LEFT
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.app_menu_shadow_length)
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding)
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .omnibox_longpress_menu_addtional_horizontal_padding),
                location[0]);
        assertEquals(
                URLBAR_BOTTOM
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_longpress_menu_overlap),
                location[1]);
    }

    @Test
    @SmallTest
    public void testCalculateShowLocationOnBottom_notRtl() {
        int[] location =
                mToolbarLongPressMenuHandler.calculateShowLocation(false, false, mBasicListMenu);
        assertEquals(
                URLBAR_LEFT
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.app_menu_shadow_length)
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding)
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .omnibox_longpress_menu_addtional_horizontal_padding),
                location[0]);
        assertEquals(
                URLBAR_TOP
                        - LONG_PRESS_MENU_HEIGHT
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_longpress_menu_overlap),
                location[1]);
    }

    @Test
    @SmallTest
    public void testCalculateShowLocationOnTop_rtl() {
        int[] location =
                mToolbarLongPressMenuHandler.calculateShowLocation(true, true, mBasicListMenu);
        assertEquals(
                URLBAR_RIGHT
                        - LONG_PRESS_MENU_WIDTH
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.app_menu_shadow_length)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .omnibox_longpress_menu_addtional_horizontal_padding),
                location[0]);
        assertEquals(
                URLBAR_BOTTOM
                        - mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_longpress_menu_overlap),
                location[1]);
    }

    @Test
    @SmallTest
    public void testCalculateShowLocationOnBottom_rtl() {
        int[] location =
                mToolbarLongPressMenuHandler.calculateShowLocation(false, true, mBasicListMenu);
        assertEquals(
                URLBAR_RIGHT
                        - LONG_PRESS_MENU_WIDTH
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.app_menu_shadow_length)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .omnibox_longpress_menu_addtional_horizontal_padding),
                location[0]);
        assertEquals(
                URLBAR_TOP
                        - LONG_PRESS_MENU_HEIGHT
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_longpress_menu_overlap),
                location[1]);
    }

    @Test
    @SmallTest
    public void testScreenDpChange_onConfigurationChanged() {
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

        verify(mTracker).notifyEvent(EventConstants.BOTTOM_TOOLBAR_MENU_TRIGGERED);

        assertNotNull(mToolbarLongPressMenuHandler.getPopupWindowForTesting());

        // Store the initial width of the popup for later verification.
        int initialMenuWidth = mToolbarLongPressMenuHandler.getPopupWindowForTesting().getWidth();

        // Act: Simulate a configuration change with a smaller screen width.
        // This simulates a screen rotation or window resizing, where the screen width is reduced.
        // Update screen width to be smaller than the initial menu width, ensuring the new menu
        // width
        // will not exceed the available screen space.
        // Note: PopupWindow#getWidth() returns pixels (Px), and in this test, we've mocked
        // DisplayAndroid
        // to return a dip scale of 1.0f (doReturn(1.0f).when(mDisplayAndroid).getDipScale()).
        // Therefore, dp and px values are equivalent for this test case.
        mConfiguration.screenWidthDp = initialMenuWidth - 1;
        mToolbarLongPressMenuHandler.onConfigurationChanged(mConfiguration);

        // Assert: Verify that the popup window is dismissed after the configuration change.
        assertFalse(mToolbarLongPressMenuHandler.getPopupWindowForTesting().isShowing());

        mToolbarLongPressMenuHandler.getOnLongClickListener().onLongClick(mUrlBar);
        verify(mSpyPopupWindow, times(2))
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        // This ensures the popup doesn't exceed the screen's bounds after a configuration change.
        assertEquals(
                initialMenuWidth - 1,
                mToolbarLongPressMenuHandler.getPopupWindowForTesting().getWidth());
    }
}

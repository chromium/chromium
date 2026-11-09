// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit tests for {@link BottomBarLongPressMenuHandler}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w360dp-h800dp-mdpi")
public class BottomBarLongPressMenuHandlerUnitTest {
    private static final int WINDOW_WIDTH = 360;
    private static final int WINDOW_HEIGHT = 800;
    private static final int BAR_HEIGHT = 56;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;

    private final Rect mLatestPopupRect = new Rect();
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private BottomBarView mBottomBarView;
    private BottomBarLongPressMenuHandler mHandler;

    @Before
    public void setUp() {
        UiWidgetFactory originalWidgetFactory = UiWidgetFactory.getInstance();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        UiWidgetFactory.setInstance(
                new UiWidgetFactory() {
                    @Override
                    public ChromePopupWindow createPopupWindow(Context context) {
                        return new ChromePopupWindow(context) {
                            @Override
                            public void update(int x, int y, int width, int height) {
                                super.update(x, y, width, height);
                                mLatestPopupRect.set(x, y, x + width, y + height);
                            }
                        };
                    }
                });
        ResettersForTesting.register(() -> UiWidgetFactory.setInstance(originalWidgetFactory));
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        FrameLayout rootLayout = new FrameLayout(mActivity);
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.bottom_bar_layout, rootLayout, false);
        FrameLayout.LayoutParams barParams =
                new FrameLayout.LayoutParams(WINDOW_WIDTH, BAR_HEIGHT, Gravity.BOTTOM);
        rootLayout.addView(mBottomBarView, barParams);
        mActivity.setContentView(rootLayout);
        layoutDecorView();
        mHandler = new BottomBarLongPressMenuHandler(mActivity, mBottomBarView);
    }

    @After
    public void tearDown() {
        mHandler.dismiss();
        mActivityController.close();
    }

    @Test
    public void testShowMenu_DisplaysCustomizeAppearanceAndNavigatesOnClick() {
        assertTrue(mHandler.showMenu());
        assertTrue(mHandler.isShowing());

        AnchoredPopupWindow popupWindow = mHandler.getPopupWindowForTesting();
        assertNotNull(popupWindow);
        View contentView = popupWindow.getContentView();
        assertNotNull(contentView);

        ListView listView = contentView.findViewById(R.id.menu_list);
        assertNotNull(listView);
        assertEquals(1, listView.getAdapter().getCount());

        View itemView = listView.getAdapter().getView(0, null, listView);
        TextView labelView = itemView.findViewById(R.id.menu_item_text);
        assertEquals(
                mActivity.getString(R.string.bottom_bar_long_press_customize_appearance),
                labelView.getText().toString());
        assertTrue(itemView.performClick());
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mHandler.isShowing());
        assertNull(mHandler.getPopupWindowForTesting());
        verify(mSettingsNavigation).startSettings(mActivity, SettingsFragment.APPEARANCE);
    }

    @Test
    public void testShowMenu_CenteredJustAboveBar() {
        int overlapPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_long_press_menu_overlap);
        int[] barLocation = new int[2];
        mBottomBarView.getLocationInWindow(barLocation);

        assertTrue(mHandler.showMenu());

        assertEquals(
                "Popup bottom must overlap the top of the bottom bar",
                barLocation[1] + overlapPx,
                mLatestPopupRect.bottom);
        assertEquals(
                "Popup must be centered on the bottom bar",
                barLocation[0] + mBottomBarView.getWidth() / 2f,
                mLatestPopupRect.exactCenterX(),
                1f);
    }

    @Test
    public void testShowMenu_PopupFailsToShow_ReturnsFalseAndClearsState() {
        AnchoredPopupWindow.setShowHookForTesting(() -> {});

        assertFalse(mHandler.showMenu());
        assertFalse(mHandler.isShowing());
        assertNull(mHandler.getPopupWindowForTesting());
    }

    @Test
    public void testShowMenu_BarNotShown_ReturnsFalse() {
        // E.g. the bottom controls went INVISIBLE while the long-press was pending.
        mBottomBarView.setVisibility(View.INVISIBLE);

        assertFalse(mHandler.showMenu());
        assertNull(mHandler.getPopupWindowForTesting());
    }

    @Test
    public void testDismissAndDestroy_ClosePopup() {
        for (Runnable close : new Runnable[] {mHandler::dismiss, mHandler::destroy}) {
            assertTrue(mHandler.showMenu());
            AnchoredPopupWindow shownPopup = mHandler.getPopupWindowForTesting();
            assertNotNull(shownPopup);

            close.run();

            assertFalse(shownPopup.isShowing());
            assertFalse(mHandler.isShowing());
        }
    }

    @Test
    public void testExternalDismiss_ClearsState() {
        assertTrue(mHandler.showMenu());
        AnchoredPopupWindow popupWindow = mHandler.getPopupWindowForTesting();
        assertNotNull(popupWindow);

        // E.g. an outside tap or Back press.
        popupWindow.onDismissForTesting(/* byInsideTouch= */ false);

        assertFalse(popupWindow.isShowing());
        assertFalse(mHandler.isShowing());
        assertNull(mHandler.getPopupWindowForTesting());
    }

    private void layoutDecorView() {
        View decorView = mActivity.getWindow().getDecorView();
        decorView.measure(
                View.MeasureSpec.makeMeasureSpec(WINDOW_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(WINDOW_HEIGHT, View.MeasureSpec.EXACTLY));
        decorView.layout(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    }
}

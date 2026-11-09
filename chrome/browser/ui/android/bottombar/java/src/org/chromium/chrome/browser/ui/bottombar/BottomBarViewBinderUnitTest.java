// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link BottomBarViewBinder} and {@link BottomBarView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private BottomBarView mBottomBarView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.bottom_bar_layout, null, false);

        mBottomBarView.getContainerForAction(ActionId.HOME_BUTTON).inflateStub();
        mBottomBarView.getContainerForAction(ActionId.GLIC).inflateStub();
        mBottomBarView.getContainerForAction(ActionId.APP_MENU).inflateStub();

        mModel = new PropertyModel.Builder(BottomBarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mBottomBarView, BottomBarViewBinder::bind);
    }

    @Test
    public void testVisibilityProperty() {
        mModel.set(BottomBarProperties.IS_VISIBLE, true);
        assertEquals(View.VISIBLE, mBottomBarView.getVisibility());

        mModel.set(BottomBarProperties.IS_VISIBLE, false);
        assertEquals(View.GONE, mBottomBarView.getVisibility());
    }

    @Test
    public void testColorSchemeProperty() {
        mModel.set(BottomBarProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        // Verifies that color scheme sets background without crashing.
        assertNotNull(mBottomBarView.getBackground());

        // Verify that the icon tints were properly updated on the children
        ColorStateList expectedTint =
                BottomBarUtils.getIconColorStateList(mActivity, BrandedColorScheme.APP_DEFAULT);
        ImageView newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);

        assertEquals(
                expectedTint.getDefaultColor(), newTabButton.getImageTintList().getDefaultColor());
    }

    @Test
    public void testButtonVisibilitiesProperty() {
        verifyButtonVisibility(BottomBarProperties.IS_HOME_BUTTON_VISIBLE, ActionId.HOME_BUTTON);
        verifyButtonVisibility(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, ActionId.GLIC);
        verifyButtonVisibility(BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE, ActionId.NEW_TAB);
        verifyButtonVisibility(
                BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE, ActionId.TAB_SWITCHER);
        verifyButtonVisibility(BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE, ActionId.APP_MENU);
    }

    private void verifyButtonVisibility(
            PropertyModel.WritableBooleanPropertyKey property, @ActionId int actionId) {
        View container = mBottomBarView.getContainerForAction(actionId);
        assertNotNull("Container should not be null for actionId: " + actionId, container);

        if (property == BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE) {
            mModel.set(BottomBarProperties.EXTRA_BUTTON_ACTION_ID, actionId);
        }

        mModel.set(property, true);
        assertEquals(View.VISIBLE, container.getVisibility());

        mModel.set(property, false);
        assertEquals(View.GONE, container.getVisibility());
    }

    @Test
    public void testNewTabBackgroundVisibilityProperty() {
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);

        mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, true);
        Drawable backgroundVisible = newTabButton.getBackground();
        assertNotNull(backgroundVisible);

        mModel.set(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE, false);
        Drawable backgroundInvisible = newTabButton.getBackground();
        assertNotNull(backgroundInvisible);

        assertNotEquals(backgroundVisible, backgroundInvisible);
    }

    @Test
    public void testLayoutOrderMatchesManagerOrder() {
        assertEquals(5, mBottomBarView.getChildCount());

        // 0: home_button_container
        assertEquals(R.id.home_button_container, mBottomBarView.getChildAt(0).getId());

        // 1: extra_button_container
        assertEquals(R.id.extra_button_container, mBottomBarView.getChildAt(1).getId());

        // 2: new tab container
        // 3: tab switcher container

        // 4: app_menu_button_container
        assertEquals(R.id.app_menu_button_container, mBottomBarView.getChildAt(4).getId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/56")
    public void testDynamicDimensions() {
        // At 56dp bar height and 1.0x density:
        // - Vertical padding centers the fixed 40dp button background within the 56dp bar
        //   and tightly wraps the boundsRespectPadding IPH highlight: (56 - 40) / 2 = 8px.
        // - Horizontal padding is fixed at 4px (R.dimen.bottom_bar_button_padding_horizontal).
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        assertEquals(4, newTabButton.getPaddingStart());
        assertEquals(8, newTabButton.getPaddingTop());
        assertEquals(4, newTabButton.getPaddingEnd());
        assertEquals(8, newTabButton.getPaddingBottom());

        View homeButton =
                mBottomBarView.getContainerForAction(ActionId.HOME_BUTTON).getTargetView();
        assertNotNull(homeButton);
        assertEquals(4, homeButton.getPaddingStart());
        assertEquals(8, homeButton.getPaddingTop());
        assertEquals(4, homeButton.getPaddingEnd());
        assertEquals(8, homeButton.getPaddingBottom());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testTopPaddingRemovedProperty() {
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View homeButton =
                mBottomBarView.getContainerForAction(ActionId.HOME_BUTTON).getTargetView();
        assertNotNull(homeButton);

        // Initially top padding is not removed (8px top and bottom at 56dp height).
        assertEquals(8, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());
        assertEquals(8, homeButton.getPaddingTop());
        assertEquals(8, homeButton.getPaddingBottom());

        // Remove top padding: topPad becomes 0, bottomPad stays 8.
        mModel.set(BottomBarProperties.IS_TOP_PADDING_REMOVED, true);
        assertEquals(0, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());
        assertEquals(0, homeButton.getPaddingTop());
        assertEquals(8, homeButton.getPaddingBottom());
        LayerDrawable newTabBackground =
                (LayerDrawable) mBottomBarView.getNewTabBackgroundForTesting();
        assertEquals(0, newTabBackground.getLayerInsetTop(0));
        assertEquals(8, newTabBackground.getLayerInsetBottom(0));

        // Ensure color scheme updates preserve the removed top padding and layer insets.
        mModel.set(BottomBarProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        assertEquals(0, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());
        assertEquals(0, homeButton.getPaddingTop());
        assertEquals(8, homeButton.getPaddingBottom());
        assertEquals(0, mBottomBarView.getNewTabRippleNoBackgroundForTesting().getLayerInsetTop(0));
        assertEquals(
                8, mBottomBarView.getNewTabRippleNoBackgroundForTesting().getLayerInsetBottom(0));
        assertEquals(0, mBottomBarView.getOtherRipplesForTesting()[0].getLayerInsetTop(0));
        assertEquals(8, mBottomBarView.getOtherRipplesForTesting()[0].getLayerInsetBottom(0));

        // Restore top padding: topPad returns to 8.
        mModel.set(BottomBarProperties.IS_TOP_PADDING_REMOVED, false);
        assertEquals(8, newTabButton.getPaddingTop());
        assertEquals(8, newTabButton.getPaddingBottom());
        assertEquals(8, homeButton.getPaddingTop());
        assertEquals(8, homeButton.getPaddingBottom());
    }

    @Test
    public void testOnLongClickListenerProperty() {
        layoutInDecorView();

        // Empty space inside the new tab container, to the left of the button.
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View newTabContainer = mBottomBarView.getContainerForAction(ActionId.NEW_TAB);
        assertNotNull(newTabContainer);
        float emptyX = newTabContainer.getLeft() + newTabButton.getLeft() / 2f;
        float midY = mBottomBarView.getHeight() / 2f;

        AtomicInteger longClickCount = new AtomicInteger();
        mModel.set(
                BottomBarProperties.ON_LONG_CLICK_LISTENER,
                v -> {
                    longClickCount.incrementAndGet();
                    return true;
                });
        assertTrue(mBottomBarView.isLongClickable());
        pressAndHold(emptyX, midY);
        assertEquals(1, longClickCount.get());

        mModel.set(BottomBarProperties.ON_LONG_CLICK_LISTENER, null);
        assertFalse(mBottomBarView.isLongClickable());
        pressAndHold(emptyX, midY);
        assertEquals(1, longClickCount.get());
    }

    @Test
    public void testOnLongClickListenerProperty_ButtonKeepsItsOwnLongPress() {
        layoutInDecorView();
        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View newTabContainer = mBottomBarView.getContainerForAction(ActionId.NEW_TAB);
        assertNotNull(newTabContainer);
        float buttonX =
                newTabContainer.getLeft() + newTabButton.getLeft() + newTabButton.getWidth() / 2f;
        float midY = mBottomBarView.getHeight() / 2f;

        AtomicInteger barLongClicks = new AtomicInteger();
        AtomicInteger buttonLongClicks = new AtomicInteger();
        mModel.set(
                BottomBarProperties.ON_LONG_CLICK_LISTENER,
                v -> {
                    barLongClicks.incrementAndGet();
                    return true;
                });
        newTabButton.setOnLongClickListener(
                v -> {
                    buttonLongClicks.incrementAndGet();
                    return true;
                });

        pressAndHold(buttonX, midY);

        // The button handles the touch itself, so the bar's menu doesn't open over it.
        assertEquals(1, buttonLongClicks.get());
        assertEquals(0, barLongClicks.get());
    }

    private void layoutInDecorView() {
        FrameLayout rootLayout = new FrameLayout(mActivity);
        rootLayout.addView(mBottomBarView, new FrameLayout.LayoutParams(1000, 100));
        mActivity.setContentView(rootLayout);
        View decorView = mActivity.getWindow().getDecorView();
        decorView.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        decorView.layout(0, 0, 1000, 800);
    }

    private void pressAndHold(float x, float y) {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent down = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0);
        mBottomBarView.dispatchTouchEvent(down);
        down.recycle();

        ShadowLooper.idleMainLooper(
                ViewConfiguration.getLongPressTimeout() + 50, TimeUnit.MILLISECONDS);

        MotionEvent up =
                MotionEvent.obtain(
                        downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, y, 0);
        mBottomBarView.dispatchTouchEvent(up);
        up.recycle();
        // Let View clear the pressed state it posts on ACTION_UP, or it cancels the next
        // long-press.
        ShadowLooper.idleMainLooper();
    }
}

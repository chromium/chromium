// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.animation.Animator;
import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.components.browser_ui.widget.ChromeTransitionDrawable;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link StatusView} and {@link StatusViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusViewUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Activity mActivity;
    private StatusView mStatusView;
    private PropertyModel mStatusModel;
    private PropertyModelChangeProcessor mStatusMCP;
    private ViewGroup mParentView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mParentView = new LinearLayout(mActivity);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mActivity.setContentView(mParentView, params);

        mStatusView =
                mActivity
                        .getLayoutInflater()
                        .inflate(R.layout.location_status, mParentView, /* attachToRoot= */ true)
                        .findViewById(R.id.location_bar_status);
        mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(mParentView));
        mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
        mStatusMCP =
                PropertyModelChangeProcessor.create(
                        mStatusModel, mStatusView, new StatusViewBinder());
        performLayout();
    }

    @After
    public void tearDown() {
        mStatusMCP.destroy();
    }

    private void performLayout() {
        mParentView.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        mParentView.layout(0, 0, 1000, 100);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testIncognitoBadgeVisibility() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        assertNull(mStatusView.findViewById(R.id.location_bar_incognito_badge));

        // Set incognito badge visible.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
        performLayout();
        View badge = mStatusView.findViewById(R.id.location_bar_incognito_badge);
        assertNotNull(badge);
        assertEquals(View.VISIBLE, badge.getVisibility());

        // Set incognito badge gone.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, false);
        performLayout();
        assertEquals(View.GONE, badge.getVisibility());
    }

    @Test
    public void testTouchDelegate_nullWhenIncognitoStatusIconInvisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        assertNull(mStatusView.findViewById(R.id.location_bar_incognito_badge));

        // Set incognito badge visible.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
        performLayout();
        View badge = mStatusView.findViewById(R.id.location_bar_incognito_badge);
        assertNotNull(badge);
        assertEquals(View.VISIBLE, badge.getVisibility());

        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        performLayout();
        assertNull(mStatusView.getTouchDelegateForTesting());
    }

    @Test
    public void testTouchDelegate_notNullWhenIncognitoStatusIconVisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        assertNull(mStatusView.findViewById(R.id.location_bar_incognito_badge));

        // Set incognito badge visible.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
        performLayout();
        View badge = mStatusView.findViewById(R.id.location_bar_incognito_badge);
        assertNotNull(badge);
        assertEquals(View.VISIBLE, badge.getVisibility());

        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_search_24dp, /* tint= */ 0));
        performLayout();
        assertNotNull(mStatusView.getTouchDelegateForTesting());
    }

    @Test
    public void statusView_goneWhenIncognitoBadgeVisible() {
        // Set location_bar_status_icon is VISIBLE in the beginning.
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_search_24dp, /* tint= */ 0));
        performLayout();
        View securityView = mStatusView.getSecurityView();
        assertEquals(View.VISIBLE, securityView.getVisibility());

        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        assertNull(mStatusView.findViewById(R.id.location_bar_incognito_badge));

        // Set incognito badge visible.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
        performLayout();
        View badge = mStatusView.findViewById(R.id.location_bar_incognito_badge);
        assertNotNull(badge);
        assertEquals(View.VISIBLE, badge.getVisibility());

        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        performLayout();
        assertEquals(View.GONE, securityView.getVisibility());
    }

    @Test
    public void testSearchEngineLogo_incognito_noMarginEnd() {
        // Set incognito badge visible.
        mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
        performLayout();
        View badge = mStatusView.findViewById(R.id.location_bar_incognito_badge);
        assertNotNull(badge);
        assertEquals(View.VISIBLE, badge.getVisibility());

        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_logo_googleg_24dp, /* tint= */ 0));
        performLayout();
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) badge.getLayoutParams();
        assertEquals(0, params.getMarginEnd());
    }

    @Test
    public void testStatusView_iconTransparencyShouldBeReset() {
        StatusIconResource statusIconResource =
                new StatusIconResource(R.drawable.ic_logo_googleg_24dp, /* tint= */ 0);
        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconResource);
        performLayout();

        // Hide the icon, this starts an animation to set alpha to 0.0.
        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        performLayout();

        // Show the icon again, the alpha property should be reset to 1.0.
        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconResource);
        performLayout();

        View securityView = mStatusView.getSecurityView();
        assertEquals(View.VISIBLE, securityView.getVisibility());
        assertEquals(1.0f, securityView.getAlpha(), 0.0f);
    }

    @Test
    public void testStatusViewAnimationStatusResetAfterDuration() {
        mStatusView.setIconAnimationDurationForTesting(50);
        mStatusModel.set(StatusProperties.ANIMATIONS_ENABLED, true);
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_logo_googleg_24dp, /* tint= */ 0));
        assertTrue(mStatusView.isStatusIconAnimating());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(mStatusView.isStatusIconAnimating());
    }

    @Test
    public void testStatusViewAnimation_noConcurrentAnimation() {
        mStatusView.setIconAnimationDurationForTesting(100);
        mStatusModel.set(StatusProperties.ANIMATIONS_ENABLED, true);
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_logo_googleg_24dp, /* tint= */ 0));
        assertTrue(mStatusView.isStatusIconAnimating());
        ChromeTransitionDrawable initialTransitionDrawable =
                (ChromeTransitionDrawable)
                        ((ImageView) mStatusView.getSecurityView()).getDrawable();
        Animator initialAnimator = initialTransitionDrawable.getAnimatorForTesting();
        assertTrue("Initial transition drawable should be animating", initialAnimator.isStarted());
        assertTrue("Initial transition drawable should be animating", initialAnimator.isRunning());
        Drawable finalDrawable = initialTransitionDrawable.getFinalDrawable();

        mStatusView.setIconAnimationDurationForTesting(0);
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_search_24dp, /* tint= */ 0));

        assertFalse(
                "Initial transition drawable should have stopped animating",
                initialAnimator.isStarted());
        assertFalse(
                "Initial transition drawable should have stopped animating",
                initialAnimator.isRunning());
        assertEquals(255, finalDrawable.getAlpha());
        assertTrue(mStatusView.isStatusIconAnimating());
        ChromeTransitionDrawable nextTransitionDrawable =
                (ChromeTransitionDrawable)
                        ((ImageView) mStatusView.getSecurityView()).getDrawable();
        assertTrue(nextTransitionDrawable.getAnimatorForTesting().isStarted());
    }

    @Test
    public void testShowStatusViewToggleVisibility() {
        mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, true);
        assertEquals(View.VISIBLE, mStatusView.getVisibility());
        mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, false);
        assertEquals(View.GONE, mStatusView.getVisibility());
    }

    @Test
    public void testShowStatusViewNotAffectedByShowIconView() {
        mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, false);
        assertEquals(View.GONE, mStatusView.getVisibility());
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_search_24dp, /* tint= */ 0));
        assertEquals(View.GONE, mStatusView.getVisibility());
    }
}

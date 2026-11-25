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
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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
    private OmniboxResourceProvider mResourceProvider;

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
        mResourceProvider = new OmniboxResourceProvider(mActivity, BrandedColorScheme.APP_DEFAULT);
        mStatusModel =
                new PropertyModel.Builder(StatusProperties.ALL_KEYS)
                        .with(StatusProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .build();
        mStatusMCP =
                PropertyModelChangeProcessor.create(
                        mStatusModel, mStatusView, new StatusViewBinder());
        performLayout();
    }

    @After
    public void tearDown() {
        mStatusMCP.destroy();
        mResourceProvider.destroy();
    }

    private void performLayout() {
        mParentView.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        mParentView.layout(0, 0, 1000, 100);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testTouchDelegate_togglesWithStatusIconVisibility() {
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(R.drawable.ic_search_24dp, /* tint= */ 0));
        performLayout();
        assertNotNull(mStatusView.getTouchDelegateForTesting());

        mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        performLayout();
        assertNull(mStatusView.getTouchDelegateForTesting());
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

    @Test
    public void testResourceProvider_setsStatusIconSize() {
        assertEquals(
                mResourceProvider.getStatusIconSize(), mStatusView.getStatusIconSizeForTesting());
    }

    @Test
    public void testUseWideStatusIconTogglesMinimumWidth() {
        mStatusModel.set(StatusProperties.USE_WIDE_STATUS_ICON, true);
        assertEquals(
                mActivity.getResources().getDimensionPixelSize(R.dimen.status_view_width_wide),
                mStatusView.getMinimumWidth());

        mStatusModel.set(StatusProperties.USE_WIDE_STATUS_ICON, false);
        assertEquals(
                mActivity.getResources().getDimensionPixelSize(R.dimen.status_view_width_narrow),
                mStatusView.getMinimumWidth());
    }

    @Test
    public void testGetStatusIconWidth_zeroWhenStatusViewGone() {
        mStatusModel.set(StatusProperties.USE_WIDE_STATUS_ICON, /* value= */ true);
        mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, /* value= */ true);
        performLayout();
        assertTrue(mStatusView.getStatusIconWidth() > 0);

        mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, /* value= */ false);
        performLayout();
        assertEquals(0, mStatusView.getStatusIconWidth());
    }

    @Test
    public void testStatusIconResource_usesResourceProviderCache() {
        assertNull(mResourceProvider.getDrawableCacheForTesting().get(R.drawable.ic_search_24dp));
        mStatusModel.set(
                StatusProperties.STATUS_ICON_RESOURCE,
                new StatusIconResource(
                        R.drawable.ic_search_24dp, R.color.default_icon_color_tint_list));
        assertNotNull(
                mResourceProvider.getDrawableCacheForTesting().get(R.drawable.ic_search_24dp));
    }
}

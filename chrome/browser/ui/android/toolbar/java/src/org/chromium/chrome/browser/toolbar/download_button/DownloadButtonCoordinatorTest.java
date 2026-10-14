// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.core.graphics.ColorUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.download_button.DownloadProgressRingDrawable.RingStatus;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link DownloadButtonCoordinator} and {@link DownloadButtonMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
public class DownloadButtonCoordinatorTest {
    private static final DownloadToolbarButtonState SHOWN_INACTIVE =
            new DownloadToolbarButtonState(
                    /* shouldShow= */ true,
                    IconState.PROGRESS,
                    /* isActive= */ false,
                    /* downloadCount= */ 1,
                    /* progressPercent= */ 50,
                    /* progressCertain= */ true);
    private static final DownloadToolbarButtonState SHOWN_ACTIVE =
            new DownloadToolbarButtonState(
                    /* shouldShow= */ true,
                    IconState.PROGRESS,
                    /* isActive= */ true,
                    /* downloadCount= */ 1,
                    /* progressPercent= */ 50,
                    /* progressCertain= */ true);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private Runnable mOnButtonClickedRunnable;
    @Mock private Runnable mOnVisibilityChangedRunnable;
    @Mock private DownloadProgressRingDrawable mRing;

    private final SettableNonNullObservableSupplier<DownloadToolbarButtonState> mStateSupplier =
            ObservableSuppliers.createNonNull(DownloadToolbarButtonState.HIDDEN);
    private IncognitoStateProvider mIncognitoStateProvider;
    private Activity mActivity;
    private ViewStub mViewStub;
    private DownloadButtonCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout root = new FrameLayout(mActivity);
        mViewStub = new ViewStub(mActivity);
        mViewStub.setLayoutResource(R.layout.download_toolbar_button);
        root.addView(mViewStub);

        mIncognitoStateProvider = new IncognitoStateProvider();

        mCoordinator =
                new DownloadButtonCoordinator(
                        mActivity,
                        mViewStub,
                        mThemeColorProvider,
                        mIncognitoStateProvider,
                        mOnButtonClickedRunnable,
                        mOnVisibilityChangedRunnable,
                        mStateSupplier);
    }

    private void show() {
        mStateSupplier.set(SHOWN_INACTIVE);
    }

    private void hide() {
        mStateSupplier.set(DownloadToolbarButtonState.HIDDEN);
    }

    private DownloadButtonView showAndGetView() {
        mCoordinator.setHasSpaceToShow(true);
        show();
        DownloadButtonView view = (DownloadButtonView) mCoordinator.getButtonView();
        assertNotNull(view);
        return view;
    }

    private ColorStateList colorStateList(int colorRes) {
        return mActivity.getColorStateList(colorRes);
    }

    private static void assertTint(ColorStateList expected, DownloadButtonView view) {
        assertEquals(
                expected.getDefaultColor(), view.getButton().getImageTintList().getDefaultColor());
    }

    @Test
    public void testInitialState_notInflatedAndNotVisible() {
        assertNull("View should not be inflated initially", mCoordinator.getButtonView());
        assertFalse("Should not be visible initially without space", mCoordinator.isVisible());
        assertFalse("Should not have space initially", mCoordinator.hasSpaceToShow());
        assertFalse("Defaults shouldShow to false", mCoordinator.shouldShow());
    }

    @Test
    public void testVisibility_requiresShouldShowAndSpace() {
        // Space available but shouldShow is false -> still not visible
        mCoordinator.setHasSpaceToShow(true);
        assertFalse("Should not be visible when shouldShow is false", mCoordinator.isVisible());
        assertNull("View should not be inflated", mCoordinator.getButtonView());

        // shouldShow = true with space -> visible and inflated
        show();
        assertTrue(
                "Should be visible when space and shouldShow are true", mCoordinator.isVisible());
        assertEquals(View.VISIBLE, mCoordinator.getButtonView().getVisibility());

        // shouldShow = false -> not visible
        hide();
        assertFalse("Should not be visible when shouldShow is false", mCoordinator.isVisible());
        assertEquals(View.GONE, mCoordinator.getButtonView().getVisibility());

        // Space lost -> hidden
        show();
        mCoordinator.setHasSpaceToShow(false);
        assertFalse("Should not be visible when no space", mCoordinator.isVisible());
        assertEquals(View.GONE, mCoordinator.getButtonView().getVisibility());
    }

    @Test
    public void testUpdateVisibility_measuresWidth() {
        int buttonWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);

        // Turn off shouldShow
        hide();
        int expectedConsumedWidth = 0;
        assertEquals(
                "Consumed width should be 0 px when not showing",
                expectedConsumedWidth,
                mCoordinator.updateVisibility(buttonWidth));
        assertFalse(mCoordinator.isVisible());
        assertNull(mCoordinator.getButtonView());

        // Turn on shouldShow
        show();
        assertEquals(
                "Consumed width should equal button width",
                buttonWidth,
                mCoordinator.updateVisibility(buttonWidth));
        assertTrue(mCoordinator.isVisible());
        assertEquals(View.VISIBLE, mCoordinator.getButtonView().getVisibility());

        // Insufficient available width
        assertEquals(
                "Consumed width should be 0 px when insufficient width",
                expectedConsumedWidth,
                mCoordinator.updateVisibility(buttonWidth - 1));
        assertFalse(mCoordinator.isVisible());
        assertEquals(View.GONE, mCoordinator.getButtonView().getVisibility());
    }

    @Test
    public void testClickHandling() {
        DownloadButtonView view = showAndGetView();
        view.getButton().performClick();
        verify(mOnButtonClickedRunnable).run();
    }

    @Test
    public void testDestroy() {
        showAndGetView();
        assertEquals(1, mIncognitoStateProvider.getObserverCountForTesting());
        mCoordinator.destroy();
        verify(mThemeColorProvider).removeTintObserver(mCoordinator);
        verify(mThemeColorProvider).removeThemeColorObserver(any());
        assertEquals(0, mIncognitoStateProvider.getObserverCountForTesting());
    }

    @Test
    public void testOnVisibilityChangedCallback() {
        show();
        verify(mOnVisibilityChangedRunnable).run();

        // A state change that keeps the button shown is not a visibility change.
        mStateSupplier.set(SHOWN_ACTIVE);
        verifyNoMoreInteractions(mOnVisibilityChangedRunnable);
    }

    @Test
    public void testOnTintChanged_updatesView() {
        DownloadButtonView view = showAndGetView();
        ColorStateList newTint = ColorStateList.valueOf(Color.RED);
        mCoordinator.onTintChanged(null, newTint, BrandedColorScheme.APP_DEFAULT);
        assertTint(newTint, view);
    }

    @Test
    public void testActiveState_usesAccentTint() {
        DownloadButtonView view = showAndGetView();
        ColorStateList themeTint = ColorStateList.valueOf(Color.RED);
        mCoordinator.onTintChanged(null, themeTint, BrandedColorScheme.APP_DEFAULT);

        mStateSupplier.set(SHOWN_ACTIVE);
        assertTint(colorStateList(R.color.default_icon_color_accent1_tint_list), view);

        // A theme change while active must not override the accent.
        ColorStateList otherThemeTint = ColorStateList.valueOf(Color.BLUE);
        mCoordinator.onTintChanged(null, otherThemeTint, BrandedColorScheme.APP_DEFAULT);
        assertTint(colorStateList(R.color.default_icon_color_accent1_tint_list), view);

        // Back to inactive restores the latest theme tint.
        mStateSupplier.set(SHOWN_INACTIVE);
        assertTint(otherThemeTint, view);
    }

    @Test
    public void testActiveState_incognitoBranded_usesIncognitoAccent() {
        DownloadButtonView view = showAndGetView();
        mCoordinator.onTintChanged(
                null, ColorStateList.valueOf(Color.RED), BrandedColorScheme.INCOGNITO);
        mStateSupplier.set(SHOWN_ACTIVE);
        assertTint(colorStateList(R.color.default_icon_color_blue_light), view);
    }

    @Test
    public void testOnIncognitoStateChanged_updatesView() {
        DownloadButtonView view = showAndGetView();

        // Initially in standard mode, verify the default icon ripple background.
        assertEquals(
                ToolbarUtils.getToolbarIconRippleId(/* isIncognito= */ false),
                shadowOf(view.getButton().getBackground()).getCreatedFromResId());

        // Switch to incognito mode and verify incognito ripple background.
        mCoordinator.onIncognitoStateChanged(true);
        assertEquals(
                ToolbarUtils.getToolbarIconRippleId(/* isIncognito= */ true),
                shadowOf(view.getButton().getBackground()).getCreatedFromResId());

        // Switch back to standard mode and verify default ripple background is restored.
        mCoordinator.onIncognitoStateChanged(false);
        assertEquals(
                ToolbarUtils.getToolbarIconRippleId(/* isIncognito= */ false),
                shadowOf(view.getButton().getBackground()).getCreatedFromResId());
    }

    @Test
    public void testStateSupplier_drivesShouldShow() {
        assertFalse("Should follow initial supplier value", mCoordinator.shouldShow());

        show();
        assertTrue("Should show when supplier state is shown", mCoordinator.shouldShow());
        verify(mOnVisibilityChangedRunnable).run();

        hide();
        assertFalse("Should hide when supplier state is hidden", mCoordinator.shouldShow());
        verify(mOnVisibilityChangedRunnable, times(2)).run();
    }

    @Test
    public void testDestroy_stopsObservingStateSupplier() {
        mCoordinator.destroy();
        show();
        assertFalse(
                "Destroyed coordinator should ignore supplier changes", mCoordinator.shouldShow());
    }

    @Test
    public void testMediator_mapsStateToIconRingAndBadge() {
        PropertyModel model = new PropertyModel.Builder(DownloadButtonProperties.ALL_KEYS).build();
        DownloadButtonMediator mediator =
                new DownloadButtonMediator(
                        mActivity, model, mThemeColorProvider, mOnButtonClickedRunnable);

        // Hidden by default: arrow-and-tray glyph, no ring, no badge.
        assertIconRingAndBadge(model, R.drawable.ic_download, RingStatus.IDLE, /* badge= */ 0);

        // Two active downloads: in-progress glyph, determinate ring, count badge.
        mediator.setState(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 2,
                        /* progressPercent= */ 40,
                        /* progressCertain= */ true));
        assertIconRingAndBadge(
                model, R.drawable.ic_download_in_progress, RingStatus.DOWNLOADING, /* badge= */ 2);
        assertEquals(40, model.get(DownloadButtonProperties.PROGRESS_PERCENT));

        // All paused: ring stays at the paused progress but the badge is hidden.
        mediator.setState(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ false,
                        /* downloadCount= */ 2,
                        /* progressPercent= */ 60,
                        /* progressCertain= */ true));
        assertIconRingAndBadge(
                model, R.drawable.ic_download_in_progress, RingStatus.DOWNLOADING, /* badge= */ 0);
        assertEquals(60, model.get(DownloadButtonProperties.PROGRESS_PERCENT));

        // Unknown total size: indeterminate ring.
        mediator.setState(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 1,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ false));
        assertIconRingAndBadge(
                model, R.drawable.ic_download_in_progress, RingStatus.SCANNING, /* badge= */ 0);

        // Complete: arrow-and-tray glyph, no ring.
        mediator.setState(
                state(
                        IconState.COMPLETE,
                        /* isActive= */ true,
                        /* downloadCount= */ 0,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ true));
        assertIconRingAndBadge(model, R.drawable.ic_download, RingStatus.IDLE, /* badge= */ 0);
    }

    @Test
    public void testView_bindsIconRingAndBadge() {
        DownloadButtonView view = showAndGetView();
        view.setRingForTesting(mRing);

        mStateSupplier.set(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 2,
                        /* progressPercent= */ 40,
                        /* progressCertain= */ true));
        assertIcon(R.drawable.ic_download_in_progress, view);
        verify(mRing).setDownloading(40);
        verify(mRing).setBadgeCount(2);

        mStateSupplier.set(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 1,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ false));
        verify(mRing).setScanning();

        // Each transition updates the ring exactly once, e.g. no stale setScanning() while the
        // progress changes on the way back to DOWNLOADING.
        mStateSupplier.set(
                state(
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 1,
                        /* progressPercent= */ 60,
                        /* progressCertain= */ true));
        verify(mRing).setDownloading(60);
        verify(mRing).setScanning();

        mStateSupplier.set(
                state(
                        IconState.COMPLETE,
                        /* isActive= */ false,
                        /* downloadCount= */ 0,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ true));
        assertIcon(R.drawable.ic_download, view);
        verify(mRing).setIdle();
    }

    @Test
    public void testRingColors_followTintAndThemeColor() {
        DownloadButtonView view = showAndGetView();
        view.setRingForTesting(mRing);
        int track = ColorUtils.setAlphaComponent(Color.RED, Math.round(255 * 0.3f));

        mCoordinator.onTintChanged(
                /* tint= */ null,
                /* activityFocusTint= */ ColorStateList.valueOf(Color.RED),
                BrandedColorScheme.APP_DEFAULT);
        verify(mRing)
                .setColors(
                        track,
                        /* progressColor= */ Color.RED,
                        /* badgeBackgroundColor= */ Color.TRANSPARENT,
                        /* badgeTextColor= */ Color.RED);

        // A theme color change without a tint change must still update the badge fill.
        ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> observer =
                ArgumentCaptor.forClass(ThemeColorProvider.ThemeColorObserver.class);
        verify(mThemeColorProvider).addThemeColorObserver(observer.capture());
        observer.getValue().onThemeColorChanged(Color.WHITE, /* shouldAnimate= */ false);
        verify(mRing)
                .setColors(
                        track,
                        /* progressColor= */ Color.RED,
                        /* badgeBackgroundColor= */ Color.WHITE,
                        /* badgeTextColor= */ Color.RED);
    }

    /**
     * Returns a shown {@link DownloadToolbarButtonState} for driving the button in tests.
     *
     * @param iconState Whether downloads are in progress or complete.
     * @param isActive Whether the icon uses the active (accent) color.
     * @param downloadCount Number of in-progress downloads; 2 or more shows the badge when active.
     * @param progressPercent Aggregate progress in [0, 100] shown by the determinate ring.
     * @param progressCertain Whether the total size is known; false shows the scanning ring.
     */
    private static DownloadToolbarButtonState state(
            @IconState int iconState,
            boolean isActive,
            int downloadCount,
            int progressPercent,
            boolean progressCertain) {
        return new DownloadToolbarButtonState(
                /* shouldShow= */ true,
                iconState,
                isActive,
                downloadCount,
                progressPercent,
                progressCertain);
    }

    private static void assertIcon(int expectedResId, DownloadButtonView view) {
        assertEquals(expectedResId, shadowOf(view.getButton().getDrawable()).getCreatedFromResId());
    }

    private static void assertIconRingAndBadge(
            PropertyModel model, int iconRes, @RingStatus int ringStatus, int badge) {
        assertEquals(iconRes, model.get(DownloadButtonProperties.ICON_RES));
        assertEquals(ringStatus, model.get(DownloadButtonProperties.RING_STATUS));
        assertEquals(badge, model.get(DownloadButtonProperties.BADGE_COUNT));
    }
}

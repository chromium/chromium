// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.widget.ToastManager;

/** Unit tests for LocationBarTablet. */
@RunWith(BaseRobolectricTestRunner.class)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class LocationBarTabletUnitTest {

    private static final float DIP_SCALE = 2.0f;
    private static final int POPUP_INSET_DP = 8;
    private static final int MIN_TABLET_WIDTH_DP = 504;
    private static final int CENTERING_THRESHOLD_DP = 16;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock DisplayAndroid mDisplay;
    private @Mock UrlBarCoordinator mUrlBarCoordinator;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock StatusCoordinator mStatusCoordinator;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock ViewOutlineProvider mOutlineProvider;

    private Activity mActivity;
    private LocationBarTablet mLocationBarTablet;
    private FrameLayout mHolderView;
    private View mContainerView;

    @Before
    public void doBeforeEachTest() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        LinearLayout contentView = new LinearLayout(mActivity);
        mHolderView = new FrameLayout(mActivity);
        mLocationBarTablet = new LocationBarTablet(mActivity, null);
        mLocationBarTablet.setBackgroundResource(
                R.drawable.modern_toolbar_tablet_text_box_background);
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        mHolderView.addView(mLocationBarTablet, params);
        mLocationBarTablet.onFinishInflate();
        LinearLayout.LayoutParams parentParams =
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        contentView.addView(mHolderView, parentParams);
        mActivity.setContentView(contentView, parentParams);
        doReturn(mDisplay).when(mWindowAndroid).getDisplay();
        doReturn(DIP_SCALE).when(mDisplay).getDipScale();
        doReturn(ChromeColors.getDefaultThemeColor(mActivity, /* isIncognito= */ false))
                .when(mLocationBarDataProvider)
                .getPrimaryColor();
        mLocationBarTablet.setHolderAndContainer(mHolderView, null);
        mLocationBarTablet.initialize(
                mAutocompleteCoordinator,
                mUrlBarCoordinator,
                mStatusCoordinator,
                mLocationBarDataProvider,
                mWindowAndroid);
    }

    @Test
    public void testOnLongClick() {
        longClickAndVerifyToast(R.id.bookmark_button, R.string.menu_bookmark);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w600dp-xhdpi")
    public void testFuseboxStateChange() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.setOutlineProvider(mOutlineProvider);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        assertEquals(LayoutParams.WRAP_CONTENT, layoutParams.height);
        // When flush on the left (x = 0), hard-clamping to screen bounds [0, availableWidth]
        // prevents expanding offscreen to the left (leftMargin = 0), directing all expansion
        // to the right margin.
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(-(minWidthPx - prefocusWidth), layoutParams.rightMargin);
        assertEquals(-expansionPx, layoutParams.topMargin);
        assertEquals(Gravity.TOP, layoutParams.gravity);
        assertEquals(expansionPx, mLocationBarTablet.getPaddingLeft());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingRight());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingTop());
        assertEquals(1.0f, mHolderView.getTranslationZ(), MathUtils.EPSILON);
        assertNull(mLocationBarTablet.getOutlineProvider());

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        layoutParams = (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        assertEquals(
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modern_toolbar_tablet_background_size),
                layoutParams.height);
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(0, layoutParams.rightMargin);
        assertEquals(0, layoutParams.topMargin);
        assertEquals(Gravity.CENTER_VERTICAL, layoutParams.gravity);
        assertEquals(0, mLocationBarTablet.getPaddingLeft());
        assertEquals(0, mLocationBarTablet.getPaddingRight());
        assertEquals(0, mLocationBarTablet.getPaddingTop());
        assertEquals(0.0f, mHolderView.getTranslationZ(), MathUtils.EPSILON);
        assertEquals(mOutlineProvider, mLocationBarTablet.getOutlineProvider());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_marginCalcs() {
        // Below minimum width, expand by delta between min width and current width
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        // Below minimum width when aligned at x = 0: leftMargin is clamped to 0 to prevent
        // offscreen expansion; rightMargin expands by the full delta to reach minWidthPx.
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(-(minWidthPx - prefocusWidth), layoutParams.rightMargin);

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);

        // Above minimum width, only expand by 8 dp inset on either side
        prefocusWidth = 1100;
        measureHolder(prefocusWidth);
        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        layoutParams = (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        int expectedMargin = -expansionPx;
        // Above minimum width when aligned at x = 0: leftMargin is clamped to 0 and rightMargin
        // expands by 2 * expansionPx.
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(-2 * expansionPx, layoutParams.rightMargin);

        // Above minimum width, relatively centered. Adjust margins to center within parent.
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        // Set currentLeft within the 16dp centering threshold around 250px (250px +/- 32px)
        // to test centering adjustments for a centered layout.
        int currentLeft = 260;
        mHolderView.setLeft(currentLeft);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.COMPACT);
        int windowWidth =
                DisplayUtil.dpToPx(
                        mDisplay,
                        mLocationBarTablet.getResources().getConfiguration().screenWidthDp);
        float centeredLeft = (float) (windowWidth - prefocusWidth) / 2;
        float delta = currentLeft - centeredLeft;

        assertEquals(expectedMargin - delta, layoutParams.leftMargin, MathUtils.EPSILON);
        assertEquals(expectedMargin + delta, layoutParams.rightMargin, MathUtils.EPSILON);

        // Update width + position to reflect the newly expanded view
        measureHolder(prefocusWidth - layoutParams.leftMargin);
        mHolderView.setLeft(currentLeft + layoutParams.leftMargin);

        int initialLeftMargin = layoutParams.leftMargin;
        int initialRightMargin = layoutParams.rightMargin;
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int recomputedPrefocusWidth = prefocusWidth - initialLeftMargin;
        int recomputedUnexpandedWidth =
                recomputedPrefocusWidth + initialLeftMargin + initialRightMargin;
        int recomputedUnexpandedLeft = currentLeft;
        int recomputedTargetWidth = recomputedUnexpandedWidth + 2 * expansionPx;
        int recomputedCenteredLeft = (windowWidth - recomputedTargetWidth) / 2;
        assertEquals(
                recomputedCenteredLeft - recomputedUnexpandedLeft,
                layoutParams.leftMargin,
                MathUtils.EPSILON);
        assertEquals(
                -((recomputedCenteredLeft + recomputedTargetWidth)
                        - (recomputedUnexpandedLeft + recomputedUnexpandedWidth)),
                layoutParams.rightMargin,
                MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT,
        OmniboxFeatureList.ANDROID_DESKTOP_AIM_GATE
    })
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_clampsToContainerWidth() {
        int containerWidthDp = 400;
        int prefocusWidthDp = 150;
        int leftPositionDp = 0;
        setupContainerAndMeasure(
                toPx(containerWidthDp), toPx(prefocusWidthDp), toPx(leftPositionDp));

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();

        // Ensure the margins force expansion to the clamped maximum spanning container edges.
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(
                toPx(-(containerWidthDp - prefocusWidthDp - leftPositionDp)),
                layoutParams.rightMargin);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_clampsToContainerWidthAndShiftsLeft() {
        int containerWidthDp = 400;
        int prefocusWidthDp = 225;
        int leftPositionDp = 25;
        setupContainerAndMeasure(
                toPx(containerWidthDp), toPx(prefocusWidthDp), toPx(leftPositionDp));

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();

        assertEquals(toPx(-leftPositionDp), layoutParams.leftMargin);
        assertEquals(
                toPx(-(containerWidthDp - prefocusWidthDp - leftPositionDp)),
                layoutParams.rightMargin);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_centersWiderLayouts() {
        int containerWidthDp = 750;
        int prefocusWidthDp = 550;
        int leftPositionDp = 110; // Leans 10dp to the right of center
        setupContainerAndMeasure(
                toPx(containerWidthDp), toPx(prefocusWidthDp), toPx(leftPositionDp));

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();

        // Target expanded width: 550dp + 2 * 8dp inset = 566dp.
        // Target centered left: (750dp - 566dp) / 2 = 92dp.
        int targetWidthDp = prefocusWidthDp + 2 * POPUP_INSET_DP;
        int targetLeftDp = (containerWidthDp - targetWidthDp) / 2;
        int expectedLeftMarginDp = targetLeftDp - leftPositionDp; // 92 - 110 = -18dp (-36px)
        int expectedRightMarginDp =
                (targetLeftDp + targetWidthDp)
                        - (leftPositionDp + prefocusWidthDp); // 658 - 660 = -2dp (+4px)

        assertEquals(toPx(expectedLeftMarginDp), layoutParams.leftMargin);
        assertEquals(toPx(-expectedRightMarginDp), layoutParams.rightMargin);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_boundsSymmetricExpansionToContainer() {
        int containerWidthDp = 600;
        int prefocusWidthDp = 300;
        int leftPositionDp = 5; // The center threshold is 150dp +/- 16dp
        setupContainerAndMeasure(
                toPx(containerWidthDp), toPx(prefocusWidthDp), toPx(leftPositionDp));

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();

        // Both sides should attempt to expand to meet the minimum width on tablets, which fits
        // within the container width, but the left side has very limited space. The right side
        // will need to compensate.
        int expansionDp = MIN_TABLET_WIDTH_DP - prefocusWidthDp;
        int expectedRightMarginDp = -(expansionDp - leftPositionDp);

        assertEquals(toPx(-leftPositionDp), layoutParams.leftMargin);
        assertEquals(toPx(expectedRightMarginDp), layoutParams.rightMargin);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_popoverLayoutMode() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        mLocationBarTablet.setFuseboxLayoutMode(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        mLocationBarTablet.setReparentedToPopover(true);
        GradientDrawable outerRect =
                (GradientDrawable)
                        ((LayerDrawable) mLocationBarTablet.getBackground())
                                .findDrawableByLayerId(R.id.focused_popup_bg);
        GradientDrawable innerRect =
                (GradientDrawable)
                        ((LayerDrawable) mLocationBarTablet.getBackground())
                                .findDrawableByLayerId(R.id.focused_popup_inner_bg);
        @ColorInt
        int expectedOuterRectColor =
                OmniboxResourceProvider.getPopoverSuggestionBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedOuterRectColor, outerRect.getColor().getDefaultColor());
        assertEquals(expectedOuterRectColor, innerRect.getColor().getDefaultColor());
        assertNull(mLocationBarTablet.getForeground());

        doReturn(true).when(mUrlBarCoordinator).hasFocus();
        View urlBar = mLocationBarTablet.findViewById(R.id.url_bar);
        urlBar.dispatchGenericMotionEvent(
                MotionEventTestUtils.createMotionEvent(
                        1,
                        1,
                        MotionEvent.ACTION_HOVER_ENTER,
                        0,
                        0,
                        InputDevice.SOURCE_CLASS_POINTER,
                        MotionEvent.TOOL_TYPE_MOUSE));
        assertNull(mLocationBarTablet.getForeground());

        mLocationBarTablet.onSpecializedFuseboxModeActivated(true);
        GlifStrokeDrawable glifStrokeDrawable =
                (GlifStrokeDrawable) mLocationBarTablet.getForeground();
        float radius =
                mLocationBarTablet
                        .getResources()
                        .getDimension(R.dimen.omnibox_suggestion_dropdown_round_corner_radius);
        assertEquals(radius, glifStrokeDrawable.getCornerRadiusForTesting(), MathUtils.EPSILON);

        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        assertEquals(0, layoutParams.topMargin);
        assertEquals(0, mLocationBarTablet.getPaddingLeft());
        assertEquals(0, mLocationBarTablet.getPaddingRight());
        assertEquals(
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.location_bar_tablet_fusebox_popover_top_padding),
                mLocationBarTablet.getPaddingTop());
        assertEquals(0, urlBar.getTranslationY(), MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxBackground_noSuggestions() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        mLocationBarTablet.onSuggestionsChanged(false);

        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(-expansionPx, layoutParams.bottomMargin);
        LayerDrawable background = (LayerDrawable) mLocationBarTablet.getBackground();
        GradientDrawable outerRect = (GradientDrawable) background.getDrawable(0);
        float cornerRadius =
                mLocationBarTablet
                        .getResources()
                        .getDimension(R.dimen.omnibox_suggestion_dropdown_round_corner_radius);
        int inset =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        assertEquals(cornerRadius, outerRect.getCornerRadius(), MathUtils.EPSILON);
        assertEquals(inset, background.getLayerInsetBottom(1));
        assertEquals(inset, mLocationBarTablet.getPaddingBottom());

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        assertEquals(0, layoutParams.bottomMargin);
        assertArrayEquals(
                new float[] {cornerRadius, cornerRadius, cornerRadius, cornerRadius, 0, 0, 0, 0},
                outerRect.getCornerRadii(),
                MathUtils.EPSILON);
        assertEquals(0, background.getLayerInsetBottom(1));
        assertEquals(0, mLocationBarTablet.getPaddingBottom());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxBackground_listScrolled() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        mLocationBarTablet.onSuggestionsListScrollOffsetChanged(expansionPx + 1);

        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(-expansionPx, layoutParams.bottomMargin);
        LayerDrawable background = (LayerDrawable) mLocationBarTablet.getBackground();
        GradientDrawable outerRect = (GradientDrawable) background.getDrawable(0);
        float cornerRadius =
                mLocationBarTablet
                        .getResources()
                        .getDimension(R.dimen.omnibox_suggestion_dropdown_round_corner_radius);
        int inset =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        assertEquals(cornerRadius, outerRect.getCornerRadius(), MathUtils.EPSILON);
        assertEquals(inset, background.getLayerInsetBottom(1));
        assertEquals(inset, mLocationBarTablet.getPaddingBottom());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testHoverDrawable() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int inset =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);

        LayerDrawable hoverDrawable = mLocationBarTablet.getHoverDrawableForTesting();
        assertEquals(inset, hoverDrawable.getLayerInsetTop(0));
        assertEquals(inset, hoverDrawable.getLayerInsetBottom(0));
        assertEquals(inset, hoverDrawable.getLayerInsetStart(0));
        assertEquals(inset, hoverDrawable.getLayerInsetEnd(0));

        int steadyStateInset =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modern_toolbar_background_vertical_offset);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        assertEquals(steadyStateInset, hoverDrawable.getLayerInsetTop(0));
        assertEquals(steadyStateInset, hoverDrawable.getLayerInsetBottom(0));
        assertEquals(0, hoverDrawable.getLayerInsetStart(0));
        assertEquals(0, hoverDrawable.getLayerInsetEnd(0));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChanged_compact() {
        View urlBar = mLocationBarTablet.findViewById(R.id.url_bar);
        View deleteButton = mLocationBarTablet.findViewById(R.id.delete_button);
        View micButton = mLocationBarTablet.findViewById(R.id.mic_button);
        View statusView = mLocationBarTablet.findViewById(R.id.location_bar_status);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.COMPACT);
        int translationY =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_url_bar_translation_y);
        assertEquals(translationY, urlBar.getTranslationY(), MathUtils.EPSILON);
        assertEquals(translationY, deleteButton.getTranslationY(), MathUtils.EPSILON);
        assertEquals(-translationY, micButton.getTranslationY(), MathUtils.EPSILON);
        assertEquals(-translationY, statusView.getTranslationY(), MathUtils.EPSILON);

        int marginTop =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_compact_status_view_top_margin);
        assertEquals(marginTop, ((MarginLayoutParams) statusView.getLayoutParams()).topMargin);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-mdpi")
    public void testWindowWidthChangedMarginCalcs() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);

        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        int expectedMargin = -((minWidthPx - prefocusWidth) / 2);
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(-(minWidthPx - prefocusWidth), layoutParams.rightMargin);

        int initialRightMargin = layoutParams.rightMargin;
        RuntimeEnvironment.setQualifiers("w599dp-mdpi");
        measureHolder(prefocusWidth);
        mLocationBarTablet.layout(
                0,
                0,
                mLocationBarTablet.getMeasuredWidth(),
                mLocationBarTablet.getMeasuredHeight());
        ShadowLooper.idleMainLooper();
        layoutParams = (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        int availableWidthPx = DisplayUtil.dpToPx(mDisplay, 599);
        int unexpandedRightPx = mHolderView.getMeasuredWidth() + initialRightMargin;
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(
                -(availableWidthPx - unexpandedRightPx),
                layoutParams.rightMargin,
                MathUtils.EPSILON);
    }

    private void longClickAndVerifyToast(int viewId, int stringId) {
        mLocationBarTablet.onLongClick(mLocationBarTablet.findViewById(viewId));
        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        mActivity.getResources().getString(stringId), R.id.toast_text));
        ToastManager.resetForTesting();
    }

    @Test
    public void testUpdateVisualsForState_unfocused() {
        // TODO(https://crbug.com/495794043): Replace with a render test.
        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.INCOGNITO);

        LocationBarBackgroundDrawable background =
                (LocationBarBackgroundDrawable) mLocationBarTablet.getBackground();
        GradientDrawable unfocusedRect = background.getBackgroundGradient();

        @ColorInt
        int expectedIncognitoColor =
                OmniboxResourceProvider.getTabletToolbarTextBoxBackgroundColor(
                        mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(expectedIncognitoColor, unfocusedRect.getColor().getDefaultColor());

        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        background = (LocationBarBackgroundDrawable) mLocationBarTablet.getBackground();
        unfocusedRect = background.getBackgroundGradient();
        @ColorInt
        int expectedAppDefaultColor =
                OmniboxResourceProvider.getTabletToolbarTextBoxBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedAppDefaultColor, unfocusedRect.getColor().getDefaultColor());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testUpdateVisualsForState_focused() {
        // TODO(https://crbug.com/495794043): Replace with a render test.
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.INCOGNITO);

        LayerDrawable background = (LayerDrawable) mLocationBarTablet.getBackground();
        GradientDrawable outerRect =
                (GradientDrawable) background.findDrawableByLayerId(R.id.focused_popup_bg);
        GradientDrawable innerRect =
                (GradientDrawable) background.findDrawableByLayerId(R.id.focused_popup_inner_bg);

        @ColorInt
        int expectedIncognitoOuterColor =
                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                        mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(expectedIncognitoOuterColor, outerRect.getColor().getDefaultColor());

        @ColorInt
        int expectedIncognitoInnerColor =
                OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                        mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(expectedIncognitoInnerColor, innerRect.getColor().getDefaultColor());

        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);

        @ColorInt
        int expectedAppDefaultOuterColor =
                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedAppDefaultOuterColor, outerRect.getColor().getDefaultColor());

        @ColorInt
        int expectedAppDefaultInnerColor =
                OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedAppDefaultInnerColor, innerRect.getColor().getDefaultColor());
    }

    @Test
    public void testSetIsInStandby() {
        assertNull(mLocationBarTablet.getForeground());
        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.APP_DEFAULT);
        LocationBarBackgroundDrawable background =
                (LocationBarBackgroundDrawable) mLocationBarTablet.getBackground();
        GradientDrawable unfocusedRect = background.getBackgroundGradient();

        mLocationBarTablet.setShowStandbyRing(true);

        // Verify the InsetDrawable border was applied to the foreground.
        assertNotNull(mLocationBarTablet.getForeground());
        assertTrue(mLocationBarTablet.getForeground() instanceof InsetDrawable);

        @ColorInt
        int expectedStandbyColor =
                OmniboxResourceProvider.getTabletToolbarTextBoxStandbyBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedStandbyColor, unfocusedRect.getColor().getDefaultColor());

        mLocationBarTablet.setShowStandbyRing(false);
        mLocationBarTablet.updateVisualsForState(BrandedColorScheme.INCOGNITO);
        mLocationBarTablet.setShowStandbyRing(true);
        @ColorInt
        int expectedIncognitoStandbyColor =
                OmniboxResourceProvider.getTabletToolbarTextBoxStandbyBackgroundColor(
                        mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(expectedIncognitoStandbyColor, unfocusedRect.getColor().getDefaultColor());

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.COMPACT);
        // Standby mode should override the fusebox state when deciding if to expand.
        var layoutParams = (LinearLayout.LayoutParams) mHolderView.getLayoutParams();
        assertEquals(0, layoutParams.leftMargin);
        assertEquals(0, layoutParams.rightMargin);
        assertEquals(0, layoutParams.topMargin);

        View urlBar = mLocationBarTablet.findViewById(R.id.url_bar);
        View statusView = mLocationBarTablet.findViewById(R.id.location_bar_status);
        assertEquals(0, urlBar.getTranslationY(), MathUtils.EPSILON);
        assertEquals(0, statusView.getTranslationY(), MathUtils.EPSILON);

        mLocationBarTablet.setShowStandbyRing(false);
        assertNull(mLocationBarTablet.getForeground());
        @ColorInt
        int expectedNormalColor =
                OmniboxResourceProvider.getTabletToolbarTextBoxBackgroundColor(
                        mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(expectedNormalColor, unfocusedRect.getColor().getDefaultColor());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testPopoverMarginsAppliedOnceAcrossReparenting() {
        int prefocusWidth = 400;
        measureHolder(prefocusWidth);
        mLocationBarTablet.setFuseboxLayoutMode(FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        LinearLayout.LayoutParams popoverLayoutParams =
                (LinearLayout.LayoutParams) mHolderView.getLayoutParams();

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        assertEquals(0, popoverLayoutParams.leftMargin);
        assertEquals(0, popoverLayoutParams.rightMargin);

        mLocationBarTablet.setReparentedToPopover(true);

        // In popover mode, margins remain 0 and target popover geometry is published via getters.
        assertEquals(0, popoverLayoutParams.leftMargin);
        assertEquals(0, popoverLayoutParams.rightMargin);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        assertEquals(minWidthPx, mLocationBarTablet.getAlignmentViewTargetWidth());
    }

    private void setupContainerAndMeasure(int containerWidth, int prefocusWidth, int leftPosition) {
        mContainerView = new View(mActivity);
        mContainerView.measure(
                MeasureSpec.makeMeasureSpec(containerWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mContainerView.layout(0, 0, containerWidth, 100);

        mLocationBarTablet.setHolderAndContainer(mHolderView, mContainerView);
        measureHolder(prefocusWidth);

        mLocationBarTablet.setOutlineProvider(mOutlineProvider);

        mHolderView.setLeft(leftPosition);
    }

    // Measuring mHolderView (FrameLayout) automatically measures child mLocationBarTablet.
    private void measureHolder(int width) {
        int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mHolderView.measure(widthSpec, heightSpec);
    }

    private int toPx(int dp) {
        return (int) (dp * DIP_SCALE);
    }
}

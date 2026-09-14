// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.lenient;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for {@link LocationBarPhone}, covering URL/status centering, width capping and focus
 * driven layout changes.
 *
 * <p>Layout is driven synchronously via {@link #showUrl}, which measures and lays the view out in
 * place. Robolectric executes the real {@code ConstraintLayout} measure/layout code on a single
 * thread, so no thread hopping, idling or polling is required.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w400dp")
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
public class LocationBarPhoneUnitTest {
    /**
     * Tolerance in pixels for centering checks, accounting for rounding accumulated across two
     * independent view boundaries (StatusView and Barrier).
     */
    private static final int LAYOUT_ROUNDING_TOLERANCE_PX = 2;

    private static final int LOCATION_BAR_HEIGHT_PX = 200;
    private static final String SHORT_URL = "google.com";
    private static final String LONG_URL =
            "https://www.google.com/search?q=" + "verylong".repeat(200);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private AutocompleteCoordinator mAutocompleteCoordinator;
    @Mock private UrlBarCoordinator mUrlBarCoordinator;
    @Mock private StatusCoordinator mStatusCoordinator;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private WindowAndroid mWindowAndroid;

    private Activity mActivity;
    private LocationBarPhone mLocationBar;
    private UrlBar mUrlBar;
    private View mStatusView;
    private int mLocationBarWidthPx;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        lenient()
                .doReturn(PageClassification.OTHER)
                .when(mLocationBarDataProvider)
                .getPageClassification(anyBoolean());
        lenient()
                .doReturn(JUnitTestGURLs.EXAMPLE_URL)
                .when(mLocationBarDataProvider)
                .getCurrentGurl();

        FrameLayout contentView = new FrameLayout(mActivity);
        mLocationBar = new LocationBarPhone(mActivity, null);
        contentView.addView(
                mLocationBar,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mActivity.setContentView(contentView);
        mLocationBar.onFinishInflate();
        mLocationBar.initialize(
                mAutocompleteCoordinator,
                mUrlBarCoordinator,
                mStatusCoordinator,
                mLocationBarDataProvider,
                mWindowAndroid);

        mUrlBar = mLocationBar.findViewById(R.id.url_bar);
        mStatusView = mLocationBar.findViewById(R.id.location_bar_status);
        // LocationBarPhone hides the group until the mediator reveals it; see
        // LocationBarMediator#onFinishNativeInitialization.
        mLocationBar.setUrlAndStatusGroupVisibility(true);
        // UrlBar keeps itself non-focusable until FirstDrawDetector reports its first draw, which
        // never occurs without a real window; grant focusability directly so that focus-driven
        // layout can be exercised.
        mUrlBar.setFocusable(true);
        mUrlBar.setFocusableInTouchMode(true);

        mLocationBarWidthPx =
                Math.round(
                        mActivity.getResources().getConfiguration().screenWidthDp
                                * mActivity.getResources().getDisplayMetrics().density);
    }

    @Test
    public void urlBarIsCentered_whenUnfocused() {
        showUrl(SHORT_URL);

        assertUrlBarCentered();
        assertFalse(mUrlBar.isHorizontallyScrollable());
        assertEquals(0, mUrlBar.getScrollX());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void urlBarFillsAvailableSpace_whenFeatureDisabled() {
        showUrl(SHORT_URL);

        assertUrlBarFillsAvailableSpace();
    }

    @Test
    public void urlBarFillsAvailableSpace_whenFocused() {
        showUrl(SHORT_URL);
        assertUrlBarCentered();

        assertTrue(mUrlBar.requestFocus());
        mLocationBar.setUrlFocusChangePercent(
                /* ntpSearchBoxScrollFraction= */ 1.0f,
                /* urlFocusChangeFraction= */ 1.0f,
                /* isUrlFocusChangeInProgress= */ false);
        layoutAtWidth(mLocationBarWidthPx);

        assertUrlBarFillsAvailableSpace();
    }

    @Test
    public void urlBarWidthIsCapped_forLongUrl() {
        showUrl(LONG_URL);

        int centeringSpace =
                mLocationBar
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_url_centering_edge_space);
        assertTrue(
                "URL bar width should be capped for long URLs",
                mUrlBar.getWidth() <= mLocationBar.getWidth() - 2 * centeringSpace);
        assertTrue(mUrlBar.isHorizontallyScrollable());
    }

    @Test
    public void urlBarShifts_whenStatusViewHidden() {
        showUrl(SHORT_URL);
        int leftWithStatusView = mUrlBar.getLeft();

        mStatusView.setVisibility(View.GONE);
        layoutAtWidth(mLocationBarWidthPx);

        assertNotEquals(
                "URL bar should move when the status view is hidden",
                leftWithStatusView,
                mUrlBar.getLeft());
        assertUrlBarCentered();
    }

    @Test
    public void urlBarRecenters_afterLongUrlReplacedByShortUrl() {
        showUrl(SHORT_URL);
        assertUrlBarCentered();
        int centeredLeft = mUrlBar.getLeft();

        showUrl(LONG_URL);
        assertNotEquals("URL bar should move when the URL grows", centeredLeft, mUrlBar.getLeft());
        assertTrue(mUrlBar.isHorizontallyScrollable());

        showUrl(SHORT_URL);
        assertUrlBarCentered();
        assertFalse(mUrlBar.isHorizontallyScrollable());
        assertEquals(0, mUrlBar.getScrollX());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void urlBarAndStatusViewAreNotTranslated_duringFocusAnimation() {
        showUrl(SHORT_URL);
        assertTrue(mUrlBar.requestFocus());

        assertNoTranslationAtFocusFraction(MathUtils.EPSILON);
        assertNoTranslationAtFocusFraction(0.5f);
        assertNoTranslationAtFocusFraction(1.0f);
    }

    /**
     * Replaces the former {@code LocationBarLayoutTest#testEnforceMinimumUrlBarWidth}, disabled on
     * device since crbug.com/359597342. That test asserted on a {@code
     * getLocationBarButtonsVisibilityForTesting()} getter which merely echoed the visibility last
     * requested by the owner and was never cleared by narrow-window suppression, so its first
     * assertion could not hold. The observable contract is the button visibility itself.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void urlActionButtonsHidden_whenLocationBarTooNarrow() {
        View micButton = mLocationBar.findViewById(R.id.mic_button);
        mLocationBar.setMicButtonVisibility(true);
        showUrl(SHORT_URL);
        assertEquals(View.VISIBLE, micButton.getVisibility());

        int minimalisticThreshold =
                mLocationBar
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_minimalistic_ui_threshold);
        layoutAtWidth(minimalisticThreshold - 1);
        assertEquals(View.GONE, micButton.getVisibility());

        // Re-requesting the action container while the window is narrow must not reveal them.
        mLocationBar.setUrlActionContainerVisibility(true);
        assertEquals(View.GONE, micButton.getVisibility());

        layoutAtWidth(mLocationBarWidthPx);
        assertEquals(View.VISIBLE, micButton.getVisibility());
    }

    /** Sets the URL bar text and runs a full, synchronous measure and layout pass. */
    private void showUrl(String url) {
        mUrlBar.setText(url);
        layoutAtWidth(mLocationBarWidthPx);
    }

    /**
     * Measures and lays the location bar out at the given width.
     *
     * <p>Two passes are required because {@link LocationBarLayout#checkUrlContainerWidth()} derives
     * the narrow-window state from {@link View#getWidth()} (the previously laid-out width) rather
     * than from the incoming measure spec, so the first pass still observes the old width. {@link
     * View#forceLayout()} is needed because {@link View#measure} short-circuits when the measure
     * spec is unchanged.
     */
    private void layoutAtWidth(int widthPx) {
        for (int pass = 0; pass < 2; pass++) {
            mLocationBar.forceLayout();
            mLocationBar.measure(
                    MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(LOCATION_BAR_HEIGHT_PX, MeasureSpec.AT_MOST));
            mLocationBar.layout(
                    0, 0, mLocationBar.getMeasuredWidth(), mLocationBar.getMeasuredHeight());
        }
    }

    private void assertNoTranslationAtFocusFraction(float urlFocusChangeFraction) {
        mLocationBar.setUrlFocusChangePercent(
                /* ntpSearchBoxScrollFraction= */ 1.0f,
                urlFocusChangeFraction,
                /* isUrlFocusChangeInProgress= */ true);

        assertEquals(0f, mUrlBar.getTranslationX(), MathUtils.EPSILON);
        assertEquals(0f, mStatusView.getTranslationX(), MathUtils.EPSILON);
    }

    /** Asserts the status view and URL bar clump is horizontally centered in the location bar. */
    private void assertUrlBarCentered() {
        boolean isStatusVisible = mStatusView.getVisibility() == View.VISIBLE;
        int leftSpace = isStatusVisible ? mStatusView.getLeft() : mUrlBar.getLeft();
        int rightSpace = mLocationBar.getWidth() - mUrlBar.getRight();

        assertEquals(
                "URL bar is not centered", leftSpace, rightSpace, LAYOUT_ROUNDING_TOLERANCE_PX);
    }

    /** Asserts the URL bar spans the whole gap between the status view and the action buttons. */
    private void assertUrlBarFillsAvailableSpace() {
        View actionButtonsSegment = mLocationBar.findViewById(R.id.action_buttons_segment);
        int availableSpace = actionButtonsSegment.getLeft() - mStatusView.getRight();

        assertEquals(
                "URL bar does not fill the available space",
                availableSpace,
                mUrlBar.getWidth(),
                LAYOUT_ROUNDING_TOLERANCE_PX);
    }
}

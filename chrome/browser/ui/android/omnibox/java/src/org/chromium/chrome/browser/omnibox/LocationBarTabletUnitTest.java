// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock WindowAndroid mWindowAndroid;
    private @Mock DisplayAndroid mDisplay;
    private @Mock UrlBarCoordinator mUrlBarCoordinator;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock StatusCoordinator mStatusCoordinator;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;

    private Activity mActivity;
    private LocationBarTablet mLocationBarTablet;

    @Before
    public void doBeforeEachTest() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        LinearLayout contentView = new LinearLayout(mActivity);
        mLocationBarTablet = new LocationBarTablet(mActivity, null);
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        contentView.addView(mLocationBarTablet, params);
        mLocationBarTablet.onFinishInflate();
        params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mActivity.setContentView(contentView, params);
        doReturn(mDisplay).when(mWindowAndroid).getDisplay();
        doReturn(DIP_SCALE).when(mDisplay).getDipScale();
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
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
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
                (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(LayoutParams.WRAP_CONTENT, layoutParams.height);
        int expectedMargin = -((minWidthPx - prefocusWidth) / 2);
        assertEquals(expectedMargin, layoutParams.leftMargin);
        assertEquals(expectedMargin, layoutParams.rightMargin);
        assertEquals(-expansionPx, layoutParams.topMargin);
        assertEquals(Gravity.TOP, layoutParams.gravity);
        assertEquals(expansionPx, mLocationBarTablet.getPaddingLeft());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingRight());
        assertEquals(expansionPx, mLocationBarTablet.getPaddingTop());
        assertEquals(1.0f, mLocationBarTablet.getTranslationZ(), MathUtils.EPSILON);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        layoutParams = (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
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
        assertEquals(0.0f, mLocationBarTablet.getTranslationZ(), MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChange_marginCalcs() {
        // Below minimum width, expand by delta between min width and current width
        int prefocusWidth = 400;
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        int expectedMargin = -((minWidthPx - prefocusWidth) / 2);
        assertEquals(expectedMargin, layoutParams.leftMargin);
        assertEquals(expectedMargin, layoutParams.rightMargin);

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);

        // Above minimum width, only expand by 12 dp inset on either side
        prefocusWidth = 1100;
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        layoutParams = (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        expectedMargin = -expansionPx;
        assertEquals(expectedMargin, layoutParams.leftMargin);
        assertEquals(expectedMargin, layoutParams.rightMargin);

        // Above minimum width, relatively centered. Adjust margins to center within parent.
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.DISABLED);
        int currentLeft = 300;
        mLocationBarTablet.setLeft(currentLeft);
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
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(
                        prefocusWidth - layoutParams.leftMargin, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mLocationBarTablet.setLeft(currentLeft + layoutParams.leftMargin);

        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        assertEquals(expectedMargin - delta, layoutParams.leftMargin, MathUtils.EPSILON);
        assertEquals(expectedMargin + delta, layoutParams.rightMargin, MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxBackground_noSuggestions() {
        int prefocusWidth = 400;
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        mLocationBarTablet.onSuggestionsChanged(false);

        int expansionPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
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

        mLocationBarTablet.onSuggestionsChanged(true);
        assertEquals(0, layoutParams.bottomMargin);
        assertArrayEquals(
                new float[] {cornerRadius, cornerRadius, cornerRadius, cornerRadius, 0, 0, 0, 0},
                outerRect.getCornerRadii(),
                MathUtils.EPSILON);
        assertEquals(0, background.getLayerInsetBottom(1));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-xhdpi")
    public void testFuseboxStateChanged_compact() {
        View urlBar = mLocationBarTablet.findViewById(R.id.url_bar);
        View deleteButton = mLocationBarTablet.findViewById(R.id.delete_button);
        View micButton = mLocationBarTablet.findViewById(R.id.mic_button);
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.COMPACT);
        int translationY =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_url_bar_translation_y);
        assertEquals(translationY, urlBar.getTranslationY(), MathUtils.EPSILON);
        assertEquals(translationY, deleteButton.getTranslationY(), MathUtils.EPSILON);
        assertEquals(-translationY, micButton.getTranslationY(), MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "w800dp-mdpi")
    public void testWindowWidthChangedMarginCalcs() {
        int prefocusWidth = 400;
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mLocationBarTablet.onFuseboxStateChanged(FuseboxState.EXPANDED);
        int minWidthPx =
                mLocationBarTablet
                        .getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        int expectedMargin = -((minWidthPx - prefocusWidth) / 2);
        assertEquals(expectedMargin, layoutParams.leftMargin);
        assertEquals(expectedMargin, layoutParams.rightMargin);

        RuntimeEnvironment.setQualifiers("w599dp-mdpi");
        mLocationBarTablet.measure(
                MeasureSpec.makeMeasureSpec(prefocusWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        mLocationBarTablet.layout(
                0,
                0,
                mLocationBarTablet.getMeasuredWidth(),
                mLocationBarTablet.getMeasuredHeight());
        ShadowLooper.idleMainLooper();
        layoutParams = (LinearLayout.LayoutParams) mLocationBarTablet.getLayoutParams();
        assertEquals(expectedMargin, layoutParams.leftMargin);
        assertEquals(
                expectedMargin - (599 * DIP_SCALE - mLocationBarTablet.getMeasuredWidth()),
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
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.Uri;
import android.util.Size;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.RemoteViews;

import androidx.annotation.LayoutRes;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivityClientImpl;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate.WidgetButtonSettings;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetProviderDelegate.WidgetVariant;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.Locale;

/** Tests for the QuickActionSearchWidgetProviderDelegate. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class QuickActionSearchWidgetProviderDelegateTest {
    @Rule
    public BaseActivityTestRule<Activity> mActivityTestRule =
            new BaseActivityTestRule<>(Activity.class);

    private View mWidgetView;
    private View mDinoWidgetView;
    private QuickActionSearchWidgetProviderDelegate mDelegate;
    private Context mContext;
    private int mDefaultWidgetWidthDp;
    private int mXSmallWidgetMinHeightDp;
    private int mSmallWidgetMinHeightDp;
    private int mMediumWidgetMinHeightDp;
    private int mDinoWidgetEdgeSizeDp;
    private SearchActivityClient mClient;

    @Mock RemoteViews mMockRemoteViews;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();

        mClient = new SearchActivityClientImpl();

        mDelegate =
                new QuickActionSearchWidgetProviderDelegate(
                        mContext,
                        IntentHandler.createTrustedOpenNewTabIntent(
                                mContext, /* incognito= */ true),
                        createDinoIntent(mContext));

        Resources res = mContext.getResources();
        float density = res.getDisplayMetrics().density;

        // Depend on device-supplied defaults to test also on different form factors.
        // The computation below infers the size specific to a particular device running tests.
        mXSmallWidgetMinHeightDp =
                (int)
                        (res.getDimension(R.dimen.quick_action_search_widget_xsmall_height)
                                / density);
        mSmallWidgetMinHeightDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_small_height) / density);
        mMediumWidgetMinHeightDp =
                (int)
                        (res.getDimension(R.dimen.quick_action_search_widget_medium_height)
                                / density);
        mDefaultWidgetWidthDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_width) / density);
        mDinoWidgetEdgeSizeDp =
                (int) (res.getDimension(R.dimen.quick_action_search_widget_dino_size) / density);

        setUpViews();
    }

    @Test
    @SmallTest
    public void testSearchBarClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule,
                () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.quick_action_search_widget_search_bar_container);
                },
                /* shouldActivityLaunchVoiceMode= */ false);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testIncognitoTabClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertIncognitoModeLaunchedAfterAction(
                mActivityTestRule,
                () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.incognito_quick_action_button);
                });
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testVoiceButtonClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule,
                () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.voice_search_quick_action_button);
                },
                /* shouldActivityLaunchVoiceMode= */ true);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testDinoButtonClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertDinoGameLaunchedAfterAction(
                mActivityTestRule,
                () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.dino_quick_action_button);
                });
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testDinoWidgetDinoButtonClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertDinoGameLaunchedAfterAction(
                mActivityTestRule,
                () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mDinoWidgetView, R.id.dino_quick_action_button);
                });
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    private void setUpViews() {
        FrameLayout parentView = new FrameLayout(mContext);

        AppWidgetManager widgetManager = AppWidgetManager.getInstance(mContext);
        SearchActivityPreferences prefs =
                new SearchActivityPreferences(
                        "EngineName", new GURL("http://engine"), true, true, true);

        Resources res = mContext.getResources();
        float density = res.getDisplayMetrics().density;

        mWidgetView =
                mDelegate
                        .createSearchWidgetRemoteViews(
                                mContext,
                                mClient,
                                prefs,
                                mDefaultWidgetWidthDp,
                                mMediumWidgetMinHeightDp)
                        .apply(mContext, null);
        mDinoWidgetView =
                mDelegate
                        .createDinoWidgetRemoteViews(
                                mContext,
                                mClient,
                                prefs,
                                mDinoWidgetEdgeSizeDp,
                                mDinoWidgetEdgeSizeDp)
                        .apply(mContext, null);
    }

    /** Test copy of {@link QuickActionSearchWidgetProvider#createDinoIntent}. */
    private static Intent createDinoIntent(final Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(UrlConstants.CHROME_DINO_URL));
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }

    static class VerticalResizeHeightVariant {
        /** Reported widget height (in distance point) for this variant. */
        public final int heightDp;

        /** String representation (for human readable logs). */
        public final String variantName;

        /** Expected Layout Resource ID for that height. */
        public final Integer layoutRes;

        VerticalResizeHeightVariant(int heightDp, String variantName, @LayoutRes int layoutRes) {
            this.heightDp = heightDp;
            this.variantName = variantName;
            this.layoutRes = layoutRes;
        }
    }

    /**
     * Test that the same delegate is used for all small size widgets. This takes under
     * consideration the fact that "dipi" and "dpi" dimensions are different
     */
    @Test
    @SmallTest
    public void testWidgetVerticalResizing() {
        // Validate all height pairs (exhausts the solution space).
        // This includes both reasonable and unreasonable pairs, ie. the assertion that the
        // MIN_HEIGHT <= MAX_HEIGHT does not have to hold true.
        // We want to be thorough and test both the lower and the upper boundary against what we
        // expect to be returned.
        VerticalResizeHeightVariant[] variants =
                new VerticalResizeHeightVariant[] {
                    // The following 2 variants "should never happen" and technically violate any
                    // assumptions that could be made about Android widget sizing, but we keep these
                    // to verify that we're not doing anything unexpected / bad, like crashing.
                    new VerticalResizeHeightVariant(
                            0, //
                            "zero",
                            R.layout.quick_action_search_widget_xsmall_layout),
                    new VerticalResizeHeightVariant(
                            mXSmallWidgetMinHeightDp - 1,
                            "XSmallMinHeightDp - 1",
                            R.layout.quick_action_search_widget_xsmall_layout),

                    // The following variants test every valid variant at its boundaries.
                    new VerticalResizeHeightVariant(
                            mXSmallWidgetMinHeightDp, //
                            "XSmallMinHeightDp",
                            R.layout.quick_action_search_widget_xsmall_layout),
                    new VerticalResizeHeightVariant(
                            mXSmallWidgetMinHeightDp + 1,
                            "XSmallMinHeightDp + 1",
                            R.layout.quick_action_search_widget_xsmall_layout),
                    new VerticalResizeHeightVariant(
                            mSmallWidgetMinHeightDp - 1, //
                            "SmallMinHeightDp - 1",
                            R.layout.quick_action_search_widget_xsmall_layout),
                    new VerticalResizeHeightVariant(
                            mSmallWidgetMinHeightDp, //
                            "SmallMinHeightDp",
                            R.layout.quick_action_search_widget_small_layout),
                    new VerticalResizeHeightVariant(
                            mSmallWidgetMinHeightDp + 1, //
                            "SmallMinHeightDp + 1",
                            R.layout.quick_action_search_widget_small_layout),
                    new VerticalResizeHeightVariant(
                            mMediumWidgetMinHeightDp - 1,
                            "MediumMinHeightDp - 1",
                            R.layout.quick_action_search_widget_small_layout),
                    new VerticalResizeHeightVariant(
                            mMediumWidgetMinHeightDp, //
                            "MediumMinHeightDp",
                            R.layout.quick_action_search_widget_medium_layout),
                    new VerticalResizeHeightVariant(
                            mMediumWidgetMinHeightDp + 1,
                            "MediumMinHeightDp + 1",
                            R.layout.quick_action_search_widget_medium_layout),
                };

        for (VerticalResizeHeightVariant variant : variants) {
            Integer layoutRes = mDelegate.getSearchWidgetVariantForHeight(variant.heightDp).layout;
            Assert.assertEquals(
                    "Unexpected layout where height=" + variant.variantName,
                    variant.layoutRes,
                    layoutRes);
        }
    }

    @Test
    @SmallTest
    public void computeNumberOfButtonsToHide_mediumWidget() {
        testComputeNumberOfButtonsToHideForVariant(mDelegate.getMediumWidgetVariantForTesting());
    }

    @Test
    @SmallTest
    public void computeNumberOfButtonsToHide_smallWidget() {
        testComputeNumberOfButtonsToHideForVariant(mDelegate.getSmallWidgetVariantForTesting());
    }

    @Test
    @SmallTest
    public void computeNumberOfButtonsToHide_extraSmallWidget() {
        testComputeNumberOfButtonsToHideForVariant(
                mDelegate.getExtraSmallWidgetVariantForTesting());
    }

    private void testComputeNumberOfButtonsToHideForVariant(WidgetVariant variant) {
        // Target area width >= reference width: widget fits fully, no buttons should be hidden.
        Assert.assertEquals(0, variant.computeNumberOfButtonsToHide(variant.widgetWidthDp * 2));
        Assert.assertEquals(0, variant.computeNumberOfButtonsToHide(variant.widgetWidthDp + 1));
        Assert.assertEquals(0, variant.computeNumberOfButtonsToHide(variant.widgetWidthDp));

        // Target area width < reference width:
        Assert.assertEquals(1, variant.computeNumberOfButtonsToHide(variant.widgetWidthDp - 1));
        Assert.assertEquals(
                1,
                variant.computeNumberOfButtonsToHide(
                        variant.widgetWidthDp - variant.buttonWidthDp));
        Assert.assertEquals(
                2,
                variant.computeNumberOfButtonsToHide(
                        variant.widgetWidthDp - variant.buttonWidthDp - 1));
        Assert.assertEquals(
                3,
                variant.computeNumberOfButtonsToHide(
                        variant.widgetWidthDp - variant.buttonWidthDp * 3));
    }

    @Test
    @SmallTest
    public void getElementSizeInDP_noMargins() {
        Resources res = mContext.getResources();

        // Convert a simple dimension into DP.
        float expectedSizeDp =
                res.getDimension(R.dimen.quick_action_search_widget_medium_button_width)
                        / res.getDisplayMetrics().density;

        // Check that the method returns that same value after conversion.
        Assert.assertEquals(
                (int) expectedSizeDp,
                WidgetVariant.getElementSizeInDP(
                        res, R.dimen.quick_action_search_widget_medium_button_width, 0));
    }

    @Test
    @SmallTest
    public void getElementSizeInDP_withMargins() {
        Resources res = mContext.getResources();

        // Convert a single dimension + surrounding margins into DP.
        float expectedSizeDp =
                (res.getDimension(R.dimen.quick_action_search_widget_medium_button_width)
                                + res.getDimension(
                                                R.dimen
                                                        .quick_action_search_widget_medium_button_horizontal_margin)
                                        * 2)
                        / res.getDisplayMetrics().density;

        // Check that the method returns that same value after conversion.
        Assert.assertEquals(
                (int) expectedSizeDp,
                WidgetVariant.getElementSizeInDP(
                        res,
                        R.dimen.quick_action_search_widget_medium_button_width,
                        R.dimen.quick_action_search_widget_medium_button_horizontal_margin));
    }

    @Test
    @SmallTest
    public void widgetButtonSettings_hide0Buttons() {
        // In the event the code requests K buttons to be hidden, but at least K buttons are already
        // hidden, the code is expected to take no additional action.
        WidgetButtonSettings settings = new WidgetButtonSettings();

        // Verify that the call to hideButtons does not reveal buttons.
        settings.hideButtons(0);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertFalse(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);

        // Retry with all buttons visible.
        settings.voiceSearchVisible = true;
        settings.incognitoModeVisible = true;
        settings.googleLensVisible = true;
        settings.dinoGameVisible = true;

        // Verify that the call to hideButtons does not hide anything if not necessary.
        settings.hideButtons(0);
        Assert.assertTrue(settings.voiceSearchVisible);
        Assert.assertTrue(settings.incognitoModeVisible);
        Assert.assertTrue(settings.googleLensVisible);
        Assert.assertTrue(settings.dinoGameVisible);
    }

    @Test
    @SmallTest
    public void widgetButtonSettings_hide1Button() {
        // In the event the code requests K buttons to be hidden, but at least K buttons are already
        // hidden, the code is expected to take no additional action.
        WidgetButtonSettings settings = new WidgetButtonSettings();

        // Mark one of the buttons as unavailable. In theory it shouldn't matter which one we pick.
        settings.voiceSearchVisible = false;
        settings.incognitoModeVisible = true;
        settings.googleLensVisible = true;
        settings.dinoGameVisible = true;

        // Tell settings we need 1 button removed.
        settings.hideButtons(1);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertTrue(settings.incognitoModeVisible);
        Assert.assertTrue(settings.googleLensVisible);
        Assert.assertTrue(settings.dinoGameVisible);

        // Mark the VoiceSearch button as available and try again.
        // The logic should hide the dino game first.
        settings.voiceSearchVisible = true;
        settings.hideButtons(1);
        Assert.assertTrue(settings.voiceSearchVisible);
        Assert.assertTrue(settings.incognitoModeVisible);
        Assert.assertTrue(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);
    }

    @Test
    @SmallTest
    public void widgetButtonSettings_hide3Button() {
        // In the event the code requests K buttons to be hidden, but at least K buttons are already
        // hidden, the code is expected to take no additional action.
        WidgetButtonSettings settings = new WidgetButtonSettings();

        // Mark one of the buttons as unavailable. In theory it shouldn't matter which one we pick.
        settings.voiceSearchVisible = false;
        settings.incognitoModeVisible = false;
        settings.googleLensVisible = false;
        settings.dinoGameVisible = true;

        // Tell settings we need 3 buttons removed.
        settings.hideButtons(3);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertFalse(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertTrue(settings.dinoGameVisible);

        // Mark the Incognito mode and Lens buttons as available and try again.
        // The logic should hide the Dino game and Google Lens.
        settings.incognitoModeVisible = true;
        settings.googleLensVisible = true;
        settings.hideButtons(3);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertTrue(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);

        // Finally, tell the logic that voice search is available and see that it removes the
        // Incognito mode.
        settings.voiceSearchVisible = true;
        settings.hideButtons(3);
        Assert.assertTrue(settings.voiceSearchVisible);
        Assert.assertFalse(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);
    }

    @Test
    @SmallTest
    public void widgetButtonSettings_hideLotsOfButtons() {
        // In the event the code requests K buttons to be hidden, but at least K buttons are already
        // hidden, the code is expected to take no additional action.
        WidgetButtonSettings settings = new WidgetButtonSettings();

        // Mark one of the buttons as unavailable. In theory it shouldn't matter which one we pick.
        settings.voiceSearchVisible = true;
        settings.incognitoModeVisible = true;
        settings.googleLensVisible = true;
        settings.dinoGameVisible = true;

        // Tell settings we need 4 or more buttons.
        settings.hideButtons(4);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertFalse(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);

        // Try again with more buttons.
        settings.voiceSearchVisible = true;
        settings.incognitoModeVisible = true;
        settings.googleLensVisible = true;
        settings.dinoGameVisible = true;
        settings.hideButtons(40);
        Assert.assertFalse(settings.voiceSearchVisible);
        Assert.assertFalse(settings.incognitoModeVisible);
        Assert.assertFalse(settings.googleLensVisible);
        Assert.assertFalse(settings.dinoGameVisible);
    }

    @Test
    @SmallTest
    public void computeWidgetAreaPaddingForDinoWidget_noPadding() {
        // Dino widget has a square shape. When offered a square cell area - no cropping should be
        // performed.
        Size s = mDelegate.computeWidgetAreaPaddingForDinoWidgetPx(100, 100, 1.f);
        Assert.assertEquals(0, s.getWidth());
        Assert.assertEquals(0, s.getHeight());
    }

    @Test
    @SmallTest
    public void computeWidgetAreaPaddingForDinoWidget_horizontalPadding() {
        // Dino widget has a square shape.
        // When offered a wider area, we should crop the sides.
        // Note that the input values are in DP, but result carries pixels.
        Size s = mDelegate.computeWidgetAreaPaddingForDinoWidgetPx(100, 80, 2.f);
        Assert.assertEquals(20, s.getWidth());
        Assert.assertEquals(0, s.getHeight());
    }

    @Test
    @SmallTest
    public void computeWidgetAreaPaddingForDinoWidget_verticalPadding() {
        // Dino widget has a square shape.
        // When offered a taller area, we should crop the top and bottom.
        // Note that the input values are in DP, but result carries pixels.
        Size s = mDelegate.computeWidgetAreaPaddingForDinoWidgetPx(100, 120, 3.f);
        Assert.assertEquals(0, s.getWidth());
        Assert.assertEquals(30, s.getHeight());
    }

    @Test
    @SmallTest
    public void computeScaleFactorForDinoWidget() {
        // The scale factor expresses how widget should grow or shrink to properly
        // fill up the space. It is computed as a proportion:
        //   scale factor = target size / reference size
        // a scale factor of 1.0 means the area will host the widget as it was designed
        // without any scaling.
        Resources r = mContext.getResources();
        Assert.assertEquals(
                1.f,
                mDelegate.computeScaleFactorForDinoWidget(
                        mDinoWidgetEdgeSizeDp, mDinoWidgetEdgeSizeDp),
                0.001f);
        Assert.assertEquals(
                1.f,
                mDelegate.computeScaleFactorForDinoWidget(
                        mDinoWidgetEdgeSizeDp, mDinoWidgetEdgeSizeDp * 2),
                0.001f);
        Assert.assertEquals(
                2.f,
                mDelegate.computeScaleFactorForDinoWidget(
                        mDinoWidgetEdgeSizeDp * 3, mDinoWidgetEdgeSizeDp * 2),
                0.001f);
        Assert.assertEquals(
                0.5f,
                mDelegate.computeScaleFactorForDinoWidget(
                        mDinoWidgetEdgeSizeDp, mDinoWidgetEdgeSizeDp / 2),
                0.001f);
    }

    @Test
    @SmallTest
    public void resizeDinoWidgetToFillTargetCellArea_centerInWideArea() {
        final Resources r = mContext.getResources();
        final float density = r.getDisplayMetrics().density;

        // Try to place the dino in the area that is half the reference size.
        // We should see the widget centered and resized accordingly.
        final int areaWidthDp = mDinoWidgetEdgeSizeDp;
        final int areaHeightDp = mDinoWidgetEdgeSizeDp / 2;
        mDelegate.resizeDinoWidgetToFillTargetCellArea(
                r, mMockRemoteViews, areaWidthDp, areaHeightDp);

        // The remaining area is half the size of the widget.
        // We divide it further by two to apply the padding on each side.
        int padSize = (int) (mDinoWidgetEdgeSizeDp * density / 2.f / 2.f);
        verify(mMockRemoteViews)
                .setViewPadding(R.id.dino_quick_action_area, padSize, 0, padSize, 0);
    }

    @Test
    @SmallTest
    public void resizeDinoWidgetToFillTargetCellArea_centerInTallArea() {
        final Resources r = mContext.getResources();
        final float density = r.getDisplayMetrics().density;

        // Try to place the dino in the area that is half the reference size.
        // We should see the widget centered and resized accordingly.
        final int areaWidthDp = mDinoWidgetEdgeSizeDp / 2;
        final int areaHeightDp = mDinoWidgetEdgeSizeDp;
        mDelegate.resizeDinoWidgetToFillTargetCellArea(
                r, mMockRemoteViews, areaWidthDp, areaHeightDp);

        // The remaining area is half the size of the widget.
        // We divide it further by two to apply the padding on each side.
        int padSize = (int) (mDinoWidgetEdgeSizeDp * density / 2.f / 2.f);
        verify(mMockRemoteViews)
                .setViewPadding(R.id.dino_quick_action_area, 0, padSize, 0, padSize);
    }

    @Test
    @SmallTest
    public void resizeDinoWidgetToFillTargetCellArea_repositionContent() {
        final Resources r = mContext.getResources();
        final float density = r.getDisplayMetrics().density;

        // Again, apply half the size of what the widget was designed for.
        final int areaWidthDp = mDinoWidgetEdgeSizeDp / 2;
        final int areaHeightDp = mDinoWidgetEdgeSizeDp;
        mDelegate.resizeDinoWidgetToFillTargetCellArea(
                r, mMockRemoteViews, areaWidthDp, areaHeightDp);

        // Since widget is half the size, the button area paddings should be half the size too.
        int imagePadLeft =
                (int) (r.getDimension(R.dimen.quick_action_search_widget_dino_padding_start) / 2.f);
        int imagePadVertical =
                (int)
                        (r.getDimension(R.dimen.quick_action_search_widget_dino_padding_vertical)
                                / 2.f);

        verify(mMockRemoteViews)
                .setViewPadding(
                        R.id.dino_quick_action_button,
                        imagePadLeft,
                        imagePadVertical,
                        0,
                        imagePadVertical);
    }

    @Test
    @SmallTest
    public void resizeDinoWidgetToFillTargetCellArea_repositionContentRTL() {
        final Configuration c = new Configuration(mContext.getResources().getConfiguration());
        c.setLayoutDirection(Locale.forLanguageTag("ar")); // arabic

        final Resources r = mContext.getResources();
        r.updateConfiguration(c, null);
        final float density = r.getDisplayMetrics().density;

        // Again, apply half the size of what the widget was designed for.
        final int areaWidthDp = mDinoWidgetEdgeSizeDp / 4;
        final int areaHeightDp = mDinoWidgetEdgeSizeDp;
        mDelegate.resizeDinoWidgetToFillTargetCellArea(
                r, mMockRemoteViews, areaWidthDp, areaHeightDp);

        // Since widget is a fraction of the size, that fraction goes into appropriate paddings.
        int imagePadRight =
                (int) (r.getDimension(R.dimen.quick_action_search_widget_dino_padding_start) / 4.f);
        int imagePadVertical =
                (int)
                        (r.getDimension(R.dimen.quick_action_search_widget_dino_padding_vertical)
                                / 4.f);

        verify(mMockRemoteViews)
                .setViewPadding(
                        R.id.dino_quick_action_button,
                        0,
                        imagePadVertical,
                        imagePadRight,
                        imagePadVertical);
    }
}

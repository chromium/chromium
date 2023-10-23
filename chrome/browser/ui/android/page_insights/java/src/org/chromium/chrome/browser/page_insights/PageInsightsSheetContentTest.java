// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * This class tests the functionality of the {@link PageInsightsSheetContent}
 * without running the coordinator/mediator.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PageInsightsSheetContentTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private BottomSheetController mBottomSheetController;
    private ScrimCoordinator mScrimCoordinator;
    private PageInsightsSheetContent mSheetContent;
    private BottomSheetTestSupport mTestSupport;
    private int mFullHeight;
    private static final float ASSERTION_DELTA = 0.01f;
    private static final long MILLIS_IN_ONE_DAY = 86400000;

    @BeforeClass
    public static void setUpSuite() {
        sTestRule.launchActivity(null);
        BottomSheetTestSupport.setSmallScreen(false);
    }

    @Before
    public void setUp() throws Exception {
        ViewGroup rootView = sTestRule.getActivity().findViewById(android.R.id.content);
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView.removeAllViews());

        mScrimCoordinator = new ScrimCoordinator(
                sTestRule.getActivity(), new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {}

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                }, rootView, Color.WHITE);

        mBottomSheetController = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
            Callback<View> initializedCallback = (v) -> {};
            return BottomSheetControllerFactory.createFullWidthBottomSheetController(scrimSupplier,
                    initializedCallback, sTestRule.getActivity().getWindow(),
                    KeyboardVisibilityDelegate.getInstance(), () -> rootView);
        });

        mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent =
                            new PageInsightsSheetContent(sTestRule.getActivity(), view -> {});
                    mBottomSheetController.requestShowContent(mSheetContent, false);
                    mFullHeight =
                            sTestRule.getActivity().getResources().getDisplayMetrics().heightPixels;
                });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mScrimCoordinator.destroy();
            mBottomSheetController = null;
        });
    }

    private void waitForAnimationToFinish() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    @Test
    @MediumTest
    public void testAvailableStates() throws Exception {
        // Starts with peeking state.
        assertEquals(BottomSheetController.SheetState.PEEK, mBottomSheetController.getSheetState());

        // Half state is disabled. Expanding sheets leads to full state.
        TestThreadUtils.runOnUiThreadBlocking(mBottomSheetController::expandSheet);
        waitForAnimationToFinish();
        assertEquals(BottomSheetController.SheetState.FULL, mBottomSheetController.getSheetState());

        // Collapsing from full state leads to peeking state.
        TestThreadUtils.runOnUiThreadBlocking(() -> mBottomSheetController.collapseSheet(false));
        assertEquals(BottomSheetController.SheetState.PEEK, mBottomSheetController.getSheetState());
    }

    @Test
    @SmallTest
    public void backButtonPressed(){
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getToolbarViewById(R.id.page_insights_back_button).performClick();
            assertEquals(View.VISIBLE,
                    getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
            assertEquals(View.GONE,
                getToolbarViewById(R.id.page_insights_child_page_header).getVisibility());
            assertEquals(View.VISIBLE,
                    getContentViewById(R.id.page_insights_feed_content).getVisibility());
            assertEquals(View.GONE,
                    getContentViewById(R.id.page_insights_child_content).getVisibility());
        });
    }

    @Test
    @SmallTest
    public void showFeedPage() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent.showFeedPage();
            assertEquals(View.VISIBLE,
                    getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
            assertEquals(View.GONE,
                    getToolbarViewById(R.id.page_insights_child_page_header).getVisibility());
            assertEquals(View.VISIBLE,
                    getContentViewById(R.id.page_insights_feed_content).getVisibility());
            assertEquals(View.GONE,
                    getContentViewById(R.id.page_insights_child_content).getVisibility());
            assertEquals(View.GONE,
                    getContentViewById(R.id.page_insights_loading_indicator).getVisibility());
        });
    }

    @Test
    @SmallTest
    public void initContent() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    mSheetContent.initContent(testView);
                    ViewGroup feedView =
                            mSheetContent
                                    .getContentView()
                                    .findViewById(R.id.page_insights_feed_content);

                    assertEquals(feedView.getChildAt(0), testView);
        });
    }

    @Test
    @MediumTest
    public void showChildPage() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String testChildPageText = "People also view";
            View testView = new View(sTestRule.getActivity());
            TextView childTextView =
                    mSheetContent.getToolbarView().findViewById(R.id.page_insights_child_title);
            ViewGroup childContentView =
                    mSheetContent.getContentView().findViewById(R.id.page_insights_child_content);

            mSheetContent.showChildPage(testView, testChildPageText);

            assertEquals(
                    View.GONE, getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
            assertEquals(View.VISIBLE,
                    getToolbarViewById(R.id.page_insights_child_page_header).getVisibility());
            assertEquals(
                    View.GONE, getContentViewById(R.id.page_insights_feed_content).getVisibility());
            assertEquals(View.VISIBLE,
                    getContentViewById(R.id.page_insights_child_content).getVisibility());
            assertEquals(childTextView.getText(), testChildPageText);
            assertEquals(childContentView.getChildAt(0), testView);
        });
    }

    @Test
    @MediumTest
    public void showLoadingIndicator() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent.showLoadingIndicator();

            assertEquals(View.VISIBLE,
                    getContentViewById(R.id.page_insights_loading_indicator).getVisibility());
            assertEquals(View.VISIBLE,
                    getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
            assertEquals(
                    View.GONE, getContentViewById(R.id.page_insights_feed_content).getVisibility());
            assertEquals(View.GONE,
                    getToolbarViewById(R.id.page_insights_child_page_header).getVisibility());
            assertEquals(View.GONE,
                    getContentViewById(R.id.page_insights_child_content).getVisibility());
        });
    }

    @Test
    @MediumTest
    public void privacyNoticeShownForFirstTime() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    mSheetContent.initContent(testView);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(
                            PageInsightsSheetContent.PEEK_HEIGHT_RATIO_WITH_PRIVACY_NOTICE,
                            ratio,
                            ASSERTION_DELTA);
                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeCloseButtonPressed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent.showFeedPage();
            getContentViewById(R.id.page_insights_privacy_notice_close_button).performClick();
            SharedPreferencesManager sharedPreferencesManager =
                    ChromeSharedPreferences.getInstance();

            Assert.assertTrue(sharedPreferencesManager.readBoolean(
                    ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, false));
            assertEquals(View.GONE,
                    getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
        });
    }

    @Test
    @MediumTest
    public void sharedPreferenceSetTruePrivacyNoticeNotShown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setPrivacyNoticePreferences(
                            true, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 1);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(
                            PageInsightsSheetContent.PEEK_HEIGHT_RATIO_WITHOUT_PRIVACY_NOTICE,
                            ratio,
                            ASSERTION_DELTA);
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeOpenedFourTimesDifferentDayNotShownOnFourthDay() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    SharedPreferencesManager sharedPreferencesManager =
                            ChromeSharedPreferences.getInstance();
                    sharedPreferencesManager.writeBoolean(
                            ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, false);
                    sharedPreferencesManager.writeInt(
                            ChromePreferenceKeys.PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT, 0);

                    for (int i = 1; i <= 4; i++) {
                        sharedPreferencesManager.writeLong(
                                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP,
                                System.currentTimeMillis() + i * MILLIS_IN_ONE_DAY);
                        mSheetContent.initContent(testView);
                        mSheetContent.showFeedPage();
                        if (i <= 3) {
                            assertEquals(
                                    View.VISIBLE,
                                    getContentViewById(R.id.page_insights_privacy_notice)
                                            .getVisibility());
                        }
                    }

                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeShownOnceEachDay() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    View testView = new View(sTestRule.getActivity());

                    mSheetContent.initContent(testView);
                    mSheetContent.showFeedPage();

                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());

                    mSheetContent.initContent(testView);
                    mSheetContent.showFeedPage();

                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    private View getToolbarViewById(int viewId) {
        return mSheetContent.getToolbarView().findViewById(viewId);
    }

    private View getContentViewById(int viewId) {
        return mSheetContent.getContentView().findViewById(viewId);
    }

    private void setPrivacyNoticePreferences(
            boolean privacyNoticeClosed,
            long privacyNoticeLastShownTimestamp,
            int numberOfTimesPrivacyNoticeShown) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, privacyNoticeClosed);
        sharedPreferencesManager.writeLong(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP,
                privacyNoticeLastShownTimestamp);
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT,
                numberOfTimesPrivacyNoticeShown);
    }
}

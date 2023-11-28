// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
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
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
 * This class tests the functionality of the {@link PageInsightsSheetContent} without running the
 * coordinator/mediator.
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
    private boolean mTapHandlerResult;
    private boolean mShouldInterceptTouchEventsResult;
    private boolean mTapHandlerCalled;
    private boolean mBackPressHandlerCalled;
    private boolean mBackPressHandlerResult;
    private static final float ASSERTION_DELTA = 0.01f;
    private static final long MILLIS_IN_ONE_DAY = 86400000;

    @BeforeClass
    public static void setUpSuite() {
        sTestRule.launchActivity(null);
        BottomSheetTestSupport.setSmallScreen(false);
    }

    @Before
    public void setUp() throws Exception {
        mTapHandlerCalled = false;
        mBackPressHandlerCalled = false;
        ViewGroup rootView = sTestRule.getActivity().findViewById(android.R.id.content);
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView.removeAllViews());

        mScrimCoordinator =
                new ScrimCoordinator(
                        sTestRule.getActivity(),
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        rootView,
                        Color.WHITE);

        mBottomSheetController =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
                            Callback<View> initializedCallback = (v) -> {};
                            return BottomSheetControllerFactory
                                    .createFullWidthBottomSheetController(
                                            scrimSupplier,
                                            initializedCallback,
                                            sTestRule.getActivity().getWindow(),
                                            KeyboardVisibilityDelegate.getInstance(),
                                            () -> rootView);
                        });

        mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
    }

    private void createSheetContent() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        createSheetContent(testValues);
    }

    private void createSheetContent(TestValues testValues) {
        FeatureList.setTestValues(testValues);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent =
                            new PageInsightsSheetContent(
                                    sTestRule.getActivity(),
                                    new View(sTestRule.getActivity()),
                                    view -> {},
                                    () -> {
                                        mBackPressHandlerCalled = true;
                                        return mBackPressHandlerResult;
                                    },
                                    new ObservableSupplierImpl<>(false),
                                    new PageInsightsSheetContent.OnBottomSheetTouchHandler() {
                                        @Override
                                        public boolean handleTap() {
                                            mTapHandlerCalled = true;
                                            return mTapHandlerResult;
                                        }

                                        @Override
                                        public boolean shouldInterceptTouchEvents() {
                                            return mShouldInterceptTouchEventsResult;
                                        }
                                    });
                    mBottomSheetController.requestShowContent(mSheetContent, false);
                    mFullHeight =
                            sTestRule.getActivity().getResources().getDisplayMetrics().heightPixels;
                });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mScrimCoordinator.destroy();
                    mBottomSheetController = null;
                });
    }

    private void waitForAnimationToFinish() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    @Test
    @SmallTest
    public void backButtonPressed_handlerCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getToolbarViewById(R.id.page_insights_back_button).performClick();
                    assertTrue(mBackPressHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void backButtonNotClicked_handlerNotCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mTapHandlerCalled);
                });
    }

    @Test
    @SmallTest
    public void handleBackPress_true() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBackPressHandlerResult = true;

                    assertTrue(mSheetContent.handleBackPress());
                });
    }

    @Test
    @SmallTest
    public void handleBackPress_false() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBackPressHandlerResult = false;

                    assertFalse(mSheetContent.handleBackPress());
                });
    }

    @Test
    @SmallTest
    public void showFeedPage() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.showFeedPage();
                    assertEquals(
                            View.VISIBLE,
                            getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
                    assertEquals(
                            View.GONE,
                            getToolbarViewById(R.id.page_insights_back_button).getVisibility());
                    assertEquals(
                            View.GONE,
                            getToolbarViewById(R.id.page_insights_child_title).getVisibility());
                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_feed_content).getVisibility());
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_child_content).getVisibility());
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_loading_indicator)
                                    .getVisibility());
                });
    }

    @Test
    @SmallTest
    public void initContent() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());

                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    ViewGroup feedView =
                            mSheetContent
                                    .getContentView()
                                    .findViewById(R.id.page_insights_feed_content);

                    assertEquals(testView, feedView.getChildAt(0));
                    int expectedHeight =
                            (int) (mFullHeight * PageInsightsSheetContent.DEFAULT_FULL_HEIGHT_RATIO)
                                    - sTestRule
                                            .getActivity()
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.page_insights_toolbar_height);
                    assertEquals(
                            expectedHeight,
                            getContentViewById(R.id.page_insights_content_container).getHeight());
                    assertEquals(expectedHeight, feedView.getHeight());
                });
    }

    @Test
    @SmallTest
    public void initContent_fullHeightFromFlag() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsSheetContent.PAGE_INSIGHTS_FULL_HEIGHT_RATIO_PARAM,
                "0.123");
        createSheetContent(testValues);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    int expectedHeight =
                            (int) (mFullHeight * 0.123)
                                    - sTestRule
                                            .getActivity()
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.page_insights_toolbar_height);
                    assertEquals(
                            expectedHeight,
                            getContentViewById(R.id.page_insights_content_container).getHeight());
                    assertEquals(
                            expectedHeight,
                            mSheetContent
                                    .getContentView()
                                    .findViewById(R.id.page_insights_feed_content)
                                    .getHeight());
                });
    }

    @Test
    @MediumTest
    public void showChildPage() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String testChildPageText = "People also view";
                    View testView = new View(sTestRule.getActivity());
                    TextView childTextView =
                            mSheetContent
                                    .getToolbarView()
                                    .findViewById(R.id.page_insights_child_title);
                    ViewGroup childContentView =
                            mSheetContent
                                    .getContentView()
                                    .findViewById(R.id.page_insights_child_content);

                    mSheetContent.showChildPage(testView, testChildPageText);

                    assertEquals(
                            View.GONE,
                            getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
                    assertEquals(
                            View.VISIBLE,
                            getToolbarViewById(R.id.page_insights_back_button).getVisibility());
                    assertEquals(
                            View.VISIBLE,
                            getToolbarViewById(R.id.page_insights_child_title).getVisibility());
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_feed_content).getVisibility());
                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_child_content).getVisibility());
                    assertEquals(childTextView.getText(), testChildPageText);
                    assertEquals(childContentView.getChildAt(0), testView);
                });
    }

    @Test
    @MediumTest
    public void showLoadingIndicator() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.showLoadingIndicator();

                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_loading_indicator)
                                    .getVisibility());
                    assertEquals(
                            View.VISIBLE,
                            getToolbarViewById(R.id.page_insights_feed_header).getVisibility());
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_feed_content).getVisibility());
                    assertEquals(
                            View.GONE,
                            getToolbarViewById(R.id.page_insights_back_button).getVisibility());
                    assertEquals(
                            View.GONE,
                            getToolbarViewById(R.id.page_insights_child_title).getVisibility());
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_child_content).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeShownForFirstTime() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(
                            PageInsightsSheetContent.DEFAULT_PEEK_WITH_PRIVACY_HEIGHT_RATIO,
                            ratio,
                            ASSERTION_DELTA);
                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());

                    int expectedFullContentHeight =
                            (int) (mFullHeight * PageInsightsSheetContent.DEFAULT_FULL_HEIGHT_RATIO)
                                    - sTestRule
                                            .getActivity()
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.page_insights_toolbar_height);
                    assertEquals(
                            expectedFullContentHeight
                                    - getContentViewById(R.id.page_insights_privacy_notice)
                                            .getHeight(),
                            getContentViewById(R.id.page_insights_feed_content).getHeight());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeShownForFirstTime_peekHeightFromFlag() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsSheetContent.PAGE_INSIGHTS_PEEK_WITH_PRIVACY_HEIGHT_RATIO_PARAM,
                "0.123");
        createSheetContent(testValues);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(0.123, ratio, ASSERTION_DELTA);
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeNotShownWhenNotRequired() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View testView = new View(sTestRule.getActivity());
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ false,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(
                            PageInsightsSheetContent.DEFAULT_PEEK_HEIGHT_RATIO,
                            ratio,
                            ASSERTION_DELTA);
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void privacyNoticeCloseButtonPressed() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();
                    getContentViewById(R.id.page_insights_privacy_notice_close_button)
                            .performClick();
                    SharedPreferencesManager sharedPreferencesManager =
                            ChromeSharedPreferences.getInstance();

                    Assert.assertTrue(
                            sharedPreferencesManager.readBoolean(
                                    ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, false));
                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void sharedPreferenceSetTruePrivacyNoticeNotShown() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setPrivacyNoticePreferences(
                            true, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 1);
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();
                    float ratio = ((float) mSheetContent.getPeekHeight()) / ((float) mFullHeight);
                    assertEquals(
                            PageInsightsSheetContent.DEFAULT_PEEK_HEIGHT_RATIO,
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
        createSheetContent();
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
                        mSheetContent.initContent(
                                testView,
                                /* isPrivacyNoticeRequired= */ true,
                                /* shouldHavePeekState= */ true);
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
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setPrivacyNoticePreferences(
                            false, System.currentTimeMillis() - MILLIS_IN_ONE_DAY, 0);
                    View testView = new View(sTestRule.getActivity());

                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();

                    assertEquals(
                            View.VISIBLE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());

                    mSheetContent.initContent(
                            testView,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();

                    assertEquals(
                            View.GONE,
                            getContentViewById(R.id.page_insights_privacy_notice).getVisibility());
                });
    }

    @Test
    @MediumTest
    public void nothingClicked_handlerNotCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    assertFalse(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void contentContainerClicked_handlerCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    getContentViewById(R.id.page_insights_content_container).callOnClick();

                    assertTrue(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void toolbarViewClicked_handlerCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    mSheetContent.getToolbarView().callOnClick();

                    assertTrue(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void
            contentContainerOnInterceptTouchEvent_actionUp_handlerTrue_trueAndTapHandlerCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mShouldInterceptTouchEventsResult = true;
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    assertTrue(
                            ((LinearLayout)
                                            getContentViewById(
                                                    R.id.page_insights_content_container))
                                    .onInterceptTouchEvent(
                                            MotionEvent.obtain(
                                                    0, 0, MotionEvent.ACTION_UP, 0, 0, 0)));
                    assertTrue(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void
            contentContainerOnInterceptTouchEvent_actionUp_handlerFalse_falseAndTapHandlerNotCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mShouldInterceptTouchEventsResult = false;
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    assertFalse(
                            ((LinearLayout)
                                            getContentViewById(
                                                    R.id.page_insights_content_container))
                                    .onInterceptTouchEvent(
                                            MotionEvent.obtain(
                                                    0, 0, MotionEvent.ACTION_UP, 0, 0, 0)));
                    assertFalse(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void
            contentContainerOnInterceptTouchEvent_actionDown_handlerTrue_trueAndTapHandlerNotCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mShouldInterceptTouchEventsResult = true;
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    assertTrue(
                            ((LinearLayout)
                                            getContentViewById(
                                                    R.id.page_insights_content_container))
                                    .onInterceptTouchEvent(
                                            MotionEvent.obtain(
                                                    0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0)));
                    assertFalse(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void
            contentContainerOnInterceptTouchEvent_actionDown_handlerFalse_falseAndTapHandlerNotCalled() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mShouldInterceptTouchEventsResult = false;
                    mSheetContent.initContent(
                            new View(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);

                    assertFalse(
                            ((LinearLayout)
                                            getContentViewById(
                                                    R.id.page_insights_content_container))
                                    .onInterceptTouchEvent(
                                            MotionEvent.obtain(
                                                    0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0)));
                    assertFalse(mTapHandlerCalled);
                });
    }

    @Test
    @MediumTest
    public void getVerticalScrollOffset_noRecyclerView_returns1() {
        createSheetContent();
        assertEquals(1, mSheetContent.getVerticalScrollOffset());
    }

    @Test
    @MediumTest
    public void getVerticalScrollOffset_recyclerViewInFeedPage_returnsItsOffset() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout feedPage = new FrameLayout(sTestRule.getActivity());
                    FrameLayout recyclerViewContainer = new FrameLayout(sTestRule.getActivity());
                    RecyclerView recyclerview = new RecyclerView(sTestRule.getActivity());
                    feedPage.addView(recyclerViewContainer);
                    recyclerViewContainer.addView(recyclerview);
                    mSheetContent.initContent(
                            feedPage,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();

                    assertEquals(0, mSheetContent.getVerticalScrollOffset());
                });
    }

    @Test
    @MediumTest
    public void getVerticalScrollOffset_recyclerViewInChildPage_returnsItsOffset() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout feedPage = new FrameLayout(sTestRule.getActivity());
                    mSheetContent.initContent(
                            feedPage,
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ true);
                    mSheetContent.showFeedPage();

                    FrameLayout childPage = new FrameLayout(sTestRule.getActivity());
                    FrameLayout recyclerViewContainer = new FrameLayout(sTestRule.getActivity());
                    RecyclerView recyclerview = new RecyclerView(sTestRule.getActivity());
                    childPage.addView(recyclerViewContainer);
                    recyclerViewContainer.addView(recyclerview);
                    mSheetContent.showChildPage(childPage, "bladybla");

                    assertEquals(0, mSheetContent.getVerticalScrollOffset());
                });
    }

    @Test
    @MediumTest
    public void getPeekHeight_shouldHavePeekState() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new FrameLayout(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ false,
                            /* shouldHavePeekState= */ true);

                    assertEquals(
                            (int)
                                    (mFullHeight
                                            * PageInsightsSheetContent.DEFAULT_PEEK_HEIGHT_RATIO),
                            mSheetContent.getPeekHeight());
                });
    }

    @Test
    @MediumTest
    public void getPeekHeight_shouldHavePeekState_peekHeightFromFlag() {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                PageInsightsSheetContent.PAGE_INSIGHTS_PEEK_HEIGHT_RATIO_PARAM,
                "0.123");
        createSheetContent(testValues);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new FrameLayout(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ false,
                            /* shouldHavePeekState= */ true);

                    assertEquals((int) (mFullHeight * 0.123), mSheetContent.getPeekHeight());
                });
    }

    @Test
    @MediumTest
    public void getPeekHeight_shouldNotHavePeekState() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new FrameLayout(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ true,
                            /* shouldHavePeekState= */ false);

                    assertEquals(
                            PageInsightsSheetContent.HeightMode.DISABLED,
                            mSheetContent.getPeekHeight());
                });
    }

    @Test
    @MediumTest
    public void getPeekHeight_shouldHavePeekStateThenShouldNot() {
        createSheetContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetContent.initContent(
                            new FrameLayout(sTestRule.getActivity()),
                            /* isPrivacyNoticeRequired= */ false,
                            /* shouldHavePeekState= */ true);

                    mSheetContent.setShouldHavePeekState(false);

                    assertEquals(
                            PageInsightsSheetContent.HeightMode.DISABLED,
                            mSheetContent.getPeekHeight());
                });
    }

    @Test
    @MediumTest
    public void swipeToDismissEnabled_true() {
        createSheetContent();
        mSheetContent.setSwipeToDismissEnabled(true);

        assertTrue(mSheetContent.swipeToDismissEnabled());
    }

    @Test
    @MediumTest
    public void swipeToDismissEnabled_false() {
        createSheetContent();
        mSheetContent.setSwipeToDismissEnabled(false);

        assertFalse(mSheetContent.swipeToDismissEnabled());
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

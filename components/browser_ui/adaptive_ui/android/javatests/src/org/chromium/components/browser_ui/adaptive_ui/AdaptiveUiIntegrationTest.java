// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.activity.ComponentDialog;
import androidx.activity.OnBackPressedCallback;
import androidx.core.view.ViewCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.CloseButtonVisibility;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.SecondaryButtonBehavior;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Integration tests for Adaptive UI on a real bottom sheet and a real ModalDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiIntegrationTest {
    private static final int ROOT_SIZE_PX = 2000;
    private static final int TALL_CUSTOM_VIEW_HEIGHT_PX = 1000;
    private static final long PERIOD_MS = 500;
    private static final long LONGER_THAN_PERIOD_MS = PERIOD_MS * 2;
    private static final int SWIPE_STEPS = 10;
    private static final long SWIPE_STEP_MS = 10;
    private static final float FLICK_DISTANCE_FRACTION = 0.15f;
    private static final int FLICK_STEPS = 5;
    private static final long FLICK_STEP_MS = 8;

    // Adaptive UI dialogs need a TITLE or DIALOG_CONTENT_DESCRIPTION, and TabModalPresenter
    // asserts the same, so title-less dialog models set DIALOG_CONTENT_DESCRIPTION.
    private static final class TestTabModalPresenter extends TabModalPresenter {
        private final FrameLayout mContainer;

        TestTabModalPresenter(Activity activity, FrameLayout container) {
            super(activity);
            mContainer = container;
        }

        @Override
        protected ViewGroup createDialogContainer() {
            return mContainer;
        }

        @Override
        protected void showDialogContainer() {
            runEnterAnimation();
        }

        @Override
        protected void removeDialogView(@Nullable PropertyModel model) {
            super.removeDialogView(model);
            mContainer.animate().cancel();
            mContainer.removeAllViews();
            mContainer.setVisibility(View.GONE);
        }

        @Override
        protected void setBrowserControlsAccess(boolean restricted) {}
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private final UnownedUserDataHost mHost = new UnownedUserDataHost();
    private final List<Integer> mShownContainers = new ArrayList<>();
    private final List<Integer> mDismissReasons = new ArrayList<>();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ScrimManager mScrimManager;
    @Mock private InsetObserver mInsetObserver;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;

    private Activity mActivity;
    private FrameLayout mTabDialogContainer;
    private FrameLayout mSheetContainer;
    private ManagedBottomSheetController mSheetController;
    private BottomSheetTestSupport mSheetSupport;
    private AppModalPresenter mDialogPresenter;
    private TestTabModalPresenter mTabDialogPresenter;
    private ModalDialogManager mDialogManager;
    private AdaptiveUiControllerImpl mController;
    private @Nullable PropertyModel mScrimModel;

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTabDialogContainer = new FrameLayout(mActivity);
        mTabDialogContainer.setVisibility(View.GONE);
        mSheetContainer = new FrameLayout(mActivity);
        FrameLayout root = new FrameLayout(mActivity);
        root.addView(mTabDialogContainer);
        root.addView(mSheetContainer);
        mActivity.setContentView(root);
        layoutRoot();

        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mHost);
        when(mInsetObserver.getSupplierForKeyboardInset())
                .thenReturn(ObservableSuppliers.createNonNull(0));
        doAnswer(inv -> onScrim(inv.getArgument(0), true)).when(mScrimManager).showScrim(any());
        doAnswer(inv -> onScrim(inv.getArgument(0), false))
                .when(mScrimManager)
                .hideScrim(any(), anyBoolean());

        mSheetController =
                BottomSheetControllerFactory.createFullWidthBottomSheetController(
                        () -> mScrimManager,
                        mActivity.getWindow(),
                        mKeyboardDelegate,
                        () -> mSheetContainer,
                        mInsetObserver);
        mSheetSupport = new BottomSheetTestSupport(mSheetController);
        mDialogPresenter = new AppModalPresenter(mActivity);
        mTabDialogPresenter = new TestTabModalPresenter(mActivity, mTabDialogContainer);
        mDialogManager = new ModalDialogManager(mDialogPresenter, ModalDialogType.APP);
        mDialogManager.registerPresenter(mTabDialogPresenter, ModalDialogType.TAB);
        mController =
                new AdaptiveUiControllerImpl(mWindowAndroid, mDialogManager, mSheetController);
    }

    @After
    public void tearDown() {
        mController.destroy();
        mDialogManager.destroy();
        mSheetController.destroy();
        mActivity.finish();
    }

    @Test
    public void testShowAndPrimaryAction_SheetAndDialog() {
        int[] sheetClicks = new int[1];
        showSheet(primaryModel(() -> sheetClicks[0]++, 0));
        assertEquals(List.of(AdaptiveUiContainer.BOTTOM_SHEET), mShownContainers);
        clickSheet(R.id.adaptive_ui_sheet_primary_button);
        assertEquals(1, sheetClicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
        assertNull(mSheetController.getCurrentSheetContent());

        int[] dialogClicks = new int[1];
        showDialog(primaryModel(() -> dialogClicks[0]++, 0));
        assertEquals(
                List.of(AdaptiveUiContainer.BOTTOM_SHEET, AdaptiveUiContainer.MODAL_DIALOG),
                mShownContainers);
        dialogButton(ButtonType.POSITIVE).performClick();
        assertEquals(1, dialogClicks[0]);
        assertEquals(
                List.of(DismissReason.PRIMARY_ACTION, DismissReason.PRIMARY_ACTION),
                mDismissReasons);
        assertFalse(mDialogManager.isShowing());
    }

    @Test
    public void testSecondaryAndCloseButtons_SheetAndDialog() {
        int[] secondaryClicks = new int[1];
        showSheet(secondaryModel(() -> secondaryClicks[0]++));
        assertEquals(View.GONE, sheetButton(R.id.adaptive_ui_sheet_close_button).getVisibility());
        clickSheet(R.id.adaptive_ui_sheet_secondary_button);
        assertEquals(1, secondaryClicks[0]);
        showDialog(secondaryModel(() -> secondaryClicks[0]++));
        assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
        dialogButton(ButtonType.NEGATIVE).performClick();
        assertEquals(2, secondaryClicks[0]);
        assertEquals(
                List.of(DismissReason.SECONDARY_ACTION, DismissReason.SECONDARY_ACTION),
                mDismissReasons);
        mDismissReasons.clear();

        showSheet(
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .build());
        clickSheet(R.id.adaptive_ui_sheet_close_button);
        showDialog(
                newModelBuilder()
                        .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.close))
                        .build());
        assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
        dialogCloseButton().performClick();
        assertEquals(
                List.of(DismissReason.CLOSE_BUTTON, DismissReason.CLOSE_BUTTON), mDismissReasons);
    }

    @Test
    public void testControllerDestroy_ClosesOpenEntriesAndDetaches() {
        showSheet(newModelBuilder().build());
        showDialog(
                newModelBuilder()
                        .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.ok))
                        .build());
        assertEquals(mController, AdaptiveUiController.from(mWindowAndroid));

        mController.destroy();

        assertEquals(List.of(DismissReason.TAB_CLOSED, DismissReason.TAB_CLOSED), mDismissReasons);
        assertNull(mSheetController.getCurrentSheetContent());
        assertFalse(mDialogManager.isShowing());
        assertNull(AdaptiveUiController.from(mWindowAndroid));
    }

    @Test
    public void testSheetQueueingBehindOtherContent() {
        TestBottomSheetContent blocker = otherSheet(UserCriticalFeature.NONE);
        assertTrue(mSheetController.requestShowContent(blocker, /* animate= */ true));
        settleSheet();
        var queuedModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .build();
        AdaptiveUiEntry queued =
                newEntry(queuedModel, AdaptiveUiScope.TRANSIENT, AdaptiveUiContainer.BOTTOM_SHEET);
        // Queued behind higher-precedence content: show() returns null until blocker hides.
        assertNull(mController.show(queued));
        assertTrue(mShownContainers.isEmpty());
        assertEquals(blocker, mSheetController.getCurrentSheetContent());
        mSheetController.hideContent(blocker, true, StateChangeReason.INTERACTION_COMPLETE);
        settleSheet();
        assertEquals(List.of(AdaptiveUiContainer.BOTTOM_SHEET), mShownContainers);
        assertEquals(SheetState.FULL, mSheetController.getSheetState());
        mController.dismiss(queued, DismissReason.NAVIGATION);
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.NAVIGATION), mDismissReasons);
    }

    @Test
    public void testSheetInitialStateHalf_SettlesAtHalfAndFullOnSmallScreen() {
        AdaptiveUiEntry entry = newHalfSheetEntry();
        assertEquals(Integer.valueOf(AdaptiveUiContainer.BOTTOM_SHEET), mController.show(entry));
        settleSheet();
        assertEquals(SheetState.HALF, mSheetController.getSheetState());
        // RESIZE_CONTENT sizes the content to the sheet, so the button bar stays on screen.
        assertTrue(
                mSheetController.getCurrentSheetContent().getContentView().getHeight()
                        <= mSheetController.getCurrentOffset());
        mController.dismiss(entry, DismissReason.NAVIGATION);
        mSheetSupport.endAllAnimations();

        BottomSheetTestSupport.setSmallScreen(true);
        entry = newHalfSheetEntry();
        assertEquals(Integer.valueOf(AdaptiveUiContainer.BOTTOM_SHEET), mController.show(entry));
        settleSheet();
        assertEquals(SheetState.FULL, mSheetController.getSheetState());
        mController.dismiss(entry, DismissReason.NAVIGATION);
        mSheetSupport.endAllAnimations();
    }

    @Test
    public void testBackPress_HandlerFirstThenDismiss_SheetAndWindowDialog() {
        int[] sheetBackHandled = new int[1];
        OnBackPressedCallback sheetHandler = backHandler(sheetBackHandled);
        showSheet(
                newModelBuilder()
                        .with(AdaptiveUiProperties.BACK_PRESS_HANDLER, sheetHandler)
                        .build());
        assertTrue(mSheetSupport.handleBackPress());
        assertEquals(1, sheetBackHandled[0]);
        assertTrue(mDismissReasons.isEmpty());
        sheetHandler.setEnabled(false);
        assertTrue(mSheetSupport.handleBackPress());
        mSheetSupport.endAllAnimations();
        assertEquals(1, sheetBackHandled[0]);
        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissReasons);

        int[] dialogBackHandled = new int[1];
        OnBackPressedCallback dialogHandler = backHandler(dialogBackHandled);
        PropertyModel dialogModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.ok))
                        .with(AdaptiveUiProperties.BACK_PRESS_HANDLER, dialogHandler)
                        .build();
        showDialog(dialogModel, AdaptiveUiScope.WINDOW);
        ComponentDialog dialog = mDialogPresenter.getDialogForTesting();
        dialog.getOnBackPressedDispatcher().onBackPressed();
        assertEquals(1, dialogBackHandled[0]);
        assertTrue(mDialogManager.isShowing());
        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissReasons);
        dialogHandler.setEnabled(false);
        dialog.getOnBackPressedDispatcher().onBackPressed();
        assertEquals(1, dialogBackHandled[0]);
        assertFalse(mDialogManager.isShowing());
        assertEquals(List.of(DismissReason.BACK_PRESS, DismissReason.BACK_PRESS), mDismissReasons);
    }

    @Test
    public void testSheetScrimTapAndSwipeDown() {
        showSheet(newModelBuilder().with(AdaptiveUiProperties.TITLE, str(R.string.ok)).build());
        assertNotNull(mScrimModel);
        mScrimModel.get(ScrimProperties.CLICK_DELEGATE).run();
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mDismissReasons);

        showSheet(newModelBuilder().with(AdaptiveUiProperties.TITLE, str(R.string.ok)).build());
        dispatchVerticalSwipe(sheetContainerView());
        mSheetSupport.endAllAnimations();
        assertEquals(
                List.of(DismissReason.TOUCH_OUTSIDE, DismissReason.SWIPE_DOWN), mDismissReasons);
    }

    @Test
    public void testSheetSelfScrollingCustomView_SwipeDownWhileScrolledKeepsSheet() {
        ScrollView body = showSheetWithSelfScrollingBody();
        body.scrollTo(0, 5 * ROOT_SIZE_PX);
        assertEquals(5 * ROOT_SIZE_PX, body.getScrollY());

        // The swipe scrolls the body instead of dragging the sheet down.
        dispatchVerticalSwipe(sheetContainerView());
        mSheetSupport.endAllAnimations();

        assertTrue(mDismissReasons.isEmpty());
        assertEquals(SheetState.FULL, mSheetController.getSheetState());
    }

    @Test
    public void testSheetSelfScrollingCustomView_SwipeDownAtTopDismisses() {
        ScrollView body = showSheetWithSelfScrollingBody();
        assertEquals(0, body.getScrollY());

        dispatchVerticalSwipe(sheetContainerView());
        mSheetSupport.endAllAnimations();

        assertEquals(List.of(DismissReason.SWIPE_DOWN), mDismissReasons);
    }

    @Test
    public void testSheetSelfScrollingCustomView_ShortBodyKeepsFooterAndButtonOnScreen() {
        // A body much shorter than the container leaves slack below the visible sheet.
        ScrollView shortBody = new ScrollView(mActivity);
        View shortContent = new View(mActivity);
        shortContent.setMinimumHeight(ROOT_SIZE_PX / 20);
        shortBody.addView(shortContent);
        showSheet(
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, shortBody)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                        .with(AdaptiveUiProperties.FOOTER_MESSAGE, str(R.string.ok))
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .build());
        assertEquals(SheetState.FULL, mSheetController.getSheetState());

        int visibleHeight = mSheetController.getCurrentOffset();
        assertTrue(visibleHeight < ROOT_SIZE_PX);
        View footer = sheetButton(R.id.adaptive_ui_sheet_footer_message);
        View primaryButton = sheetButton(R.id.adaptive_ui_sheet_primary_button);
        assertTrue(footer.getHeight() > 0);
        assertTrue(primaryButton.getHeight() > 0);
        assertTrue(bottomInSheetContent(footer) <= visibleHeight);
        assertTrue(bottomInSheetContent(primaryButton) <= visibleHeight);
    }

    @Test
    public void testGestureDismissalDisabled_SheetBlocksFlingWhileScrimDragAndBackDismiss() {
        // Baseline: a short fast downward flick (< 30% distance) dismisses when enabled.
        showSheet(tallSheetModel(/* gestureDismissalDisabled= */ false));
        dispatchShortFastDownwardFlick(sheetContainerView());
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mDismissReasons);
        mDismissReasons.clear();

        // With GESTURE_DISMISSAL_DISABLED, the same short fast flick snaps back to FULL.
        PropertyModel disabledModel = tallSheetModel(/* gestureDismissalDisabled= */ true);
        showSheet(disabledModel);
        dispatchShortFastDownwardFlick(sheetContainerView());
        mSheetSupport.endAllAnimations();
        assertTrue(mDismissReasons.isEmpty());
        assertEquals(SheetState.FULL, mSheetController.getSheetState());

        // Re-enabling live allows the same fast flick to dismiss.
        disabledModel.set(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, false);
        dispatchShortFastDownwardFlick(sheetContainerView());
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mDismissReasons);
        mDismissReasons.clear();

        // Scrim tap, full drag past threshold, and Back press still close non-peeking sheets.
        showSheet(tallSheetModel(/* gestureDismissalDisabled= */ true));
        assertNotNull(mScrimModel);
        mScrimModel.get(ScrimProperties.CLICK_DELEGATE).run();
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mDismissReasons);
        mDismissReasons.clear();

        showSheet(tallSheetModel(/* gestureDismissalDisabled= */ true));
        dispatchVerticalSwipe(sheetContainerView());
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mDismissReasons);
        mDismissReasons.clear();

        showSheet(tallSheetModel(/* gestureDismissalDisabled= */ true));
        assertTrue(mSheetSupport.handleBackPress());
        mSheetSupport.endAllAnimations();
        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissReasons);
    }

    @Test
    public void testGestureDismissalDisabled_DialogBlocksOutsideTouchWhileBackDismisses() {
        // Baseline: outside touch dismisses the dialog by default.
        showDialog(
                newModelBuilder().with(AdaptiveUiProperties.TITLE, str(R.string.ok)).build(),
                AdaptiveUiScope.WINDOW);
        assertTrue(dispatchDialogOutsideTouch());
        assertFalse(mDialogManager.isShowing());
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mDismissReasons);
        mDismissReasons.clear();

        // With GESTURE_DISMISSAL_DISABLED, outside touch is ignored until re-enabled live.
        PropertyModel disabledModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                        .with(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, true)
                        .build();
        showDialog(disabledModel, AdaptiveUiScope.WINDOW);
        assertFalse(dispatchDialogOutsideTouch());
        assertTrue(mDialogManager.isShowing());
        assertTrue(mDismissReasons.isEmpty());

        disabledModel.set(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, false);
        assertTrue(dispatchDialogOutsideTouch());
        assertFalse(mDialogManager.isShowing());
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mDismissReasons);
        mDismissReasons.clear();

        // Back press still closes the dialog when GESTURE_DISMISSAL_DISABLED is true.
        PropertyModel backModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                        .with(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, true)
                        .build();
        showDialog(backModel, AdaptiveUiScope.WINDOW);
        assertFalse(dispatchDialogOutsideTouch());
        mDialogPresenter.getDialogForTesting().getOnBackPressedDispatcher().onBackPressed();
        assertFalse(mDialogManager.isShowing());
        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissReasons);
    }

    @Test
    public void testSheetAccessibilityPaneTitle_CustomAndGenericFallback() {
        String generic = str(R.string.bottom_sheet_accessibility_description);
        // Pair one unset and one custom ID per entry so every open/close mutates the pane title.
        var closedModel =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                R.string.cancel)
                        .build();
        AdaptiveUiEntry first = showSheet(closedModel);
        View sheetView = sheetContainerView();
        assertEquals(generic, ViewCompat.getAccessibilityPaneTitle(sheetView));
        mController.dismiss(first, DismissReason.CLOSE_BUTTON);
        mSheetSupport.endAllAnimations();
        assertEquals(str(R.string.cancel), ViewCompat.getAccessibilityPaneTitle(sheetView));

        var openedModel =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                R.string.ok)
                        .build();
        AdaptiveUiEntry second = showSheet(openedModel);
        assertEquals(str(R.string.ok), ViewCompat.getAccessibilityPaneTitle(sheetView));
        mController.dismiss(second, DismissReason.CLOSE_BUTTON);
        mSheetSupport.endAllAnimations();
        assertEquals(generic, ViewCompat.getAccessibilityPaneTitle(sheetView));
    }

    @Test
    public void testSheetTapProtection_IgnoredDuringSlideInEvenPastPeriod() {
        int[] clicks = new int[1];
        var entry =
                newEntry(
                        primaryModel(() -> clicks[0]++, PERIOD_MS),
                        AdaptiveUiScope.TRANSIENT,
                        AdaptiveUiContainer.BOTTOM_SHEET);
        // Leave the open animation running (SCROLLING) while advancing clock past PERIOD_MS.
        assertEquals(Integer.valueOf(AdaptiveUiContainer.BOTTOM_SHEET), mController.show(entry));
        assertEquals(SheetState.SCROLLING, mSheetController.getSheetState());
        mFakeTime.advanceMillis(LONGER_THAN_PERIOD_MS);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, clicks[0]);
        assertTrue(mDismissReasons.isEmpty());
        // Advance past PERIOD_MS before settle, then inside window after settle.
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        settleSheet();
        mFakeTime.advanceMillis(PERIOD_MS - 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        clickSheet(R.id.adaptive_ui_sheet_primary_button);
        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
    }

    @Test
    public void testSheetTapProtection_PrematureTapRestartsPeriodAfterSettle() {
        int[] clicks = new int[1];
        showSheet(primaryModel(() -> clicks[0]++, PERIOD_MS));
        // Premature tap inside window is ignored and restarts the PERIOD_MS window.
        mFakeTime.advanceMillis(PERIOD_MS - 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS - 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, clicks[0]);
        // Exact inclusive boundary (PERIOD_MS) also stays blocked and resets.
        mFakeTime.advanceMillis(PERIOD_MS);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        clickSheet(R.id.adaptive_ui_sheet_primary_button);
        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
    }

    @Test
    public void testSheetTapProtection_PreemptedAndReshownBlocksUntilSettledAndRestarts() {
        int[] clicks = new int[1];
        PropertyModel model = primaryModel(() -> clicks[0]++, PERIOD_MS);
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true);
        showSheet(model);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(1, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        // User-critical non-persistent sheet preempts the open sheet and re-queues it.
        var preemptor = otherSheet(UserCriticalFeature.SECURE_PAYMENT_CONFIRMATION);
        assertTrue(mSheetController.requestShowContent(preemptor, /* animate= */ true));
        settleSheet();
        assertEquals(preemptor, mSheetController.getCurrentSheetContent());
        // Hide preemptor without animation so the re-shown entry stays in SCROLLING slide-in.
        mSheetController.hideContent(preemptor, false, StateChangeReason.INTERACTION_COMPLETE);
        assertEquals(SheetState.SCROLLING, mSheetController.getSheetState());
        mFakeTime.advanceMillis(LONGER_THAN_PERIOD_MS);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(1, clicks[0]);

        // Advance past PERIOD_MS before settle, then inside fresh post-settle window.
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        settleSheet();
        mFakeTime.advanceMillis(PERIOD_MS - 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(1, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        sheetButton(R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(2, clicks[0]);
    }

    @Test
    public void testDialogTapProtection_BlockedInsidePeriodAndWorksAfter() {
        int[] clicks = new int[1];
        showDialog(primaryModel(() -> clicks[0]++, PERIOD_MS));
        // Immediate tap is blocked regardless of enter-animation offset, anchoring period to now.
        dialogButton(ButtonType.POSITIVE).performClick();
        assertEquals(0, clicks[0]);
        assertTrue(mDialogManager.isShowing());
        mFakeTime.advanceMillis(PERIOD_MS - 1);
        dialogButton(ButtonType.POSITIVE).performClick();
        assertEquals(0, clicks[0]);
        mFakeTime.advanceMillis(PERIOD_MS + 1);
        dialogButton(ButtonType.POSITIVE).performClick();
        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
    }

    @Test
    public void testSheetButtonsFilterObscuredTouchesAndAcceptNormalTouches() {
        verifyObscuredTouchFiltering(/* isSheet= */ true);
    }

    @Test
    public void testDialogButtonsFilterObscuredTouchesAndAcceptNormalTouches() {
        verifyObscuredTouchFiltering(/* isSheet= */ false);
    }

    @Test
    public void testDialogCloseButtonVisibility_DefaultAndHideOptOut() {
        int[] secondaryClicks = new int[1];
        HistogramWatcher closeWatcher =
                watchDismissReason(/* isDialog= */ true, DismissReason.CLOSE_BUTTON);
        showDialog(secondaryModel(() -> secondaryClicks[0]++));
        assertEquals(ModalDialogType.TAB, mDialogManager.getCurrentType());
        assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
        assertEquals(str(R.string.close), dialogCloseButton().getContentDescription());
        assertEquals(View.VISIBLE, dialogButton(ButtonType.NEGATIVE).getVisibility());
        dialogCloseButton().performClick();
        closeWatcher.assertExpected();
        assertEquals(0, secondaryClicks[0]);
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDismissReasons);
        mDismissReasons.clear();

        PropertyModel shownModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .build();
        showDialog(shownModel);
        assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
        dialogCloseButton().performClick();
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDismissReasons);
        mDismissReasons.clear();

        PropertyModel hideModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.HIDDEN)
                        .build();
        showDialog(hideModel);
        assertEquals(View.GONE, dialogCloseButton().getVisibility());
        dialogButton(ButtonType.POSITIVE).performClick();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
    }

    @Test
    public void testCloseSecondaryBehavior_ParityAcrossSheetAndTabAndWindowDialogs() {
        int[] sheetClicks = new int[1];
        HistogramWatcher sheetSecondaryWatcher =
                watchDismissReason(/* isDialog= */ false, DismissReason.SECONDARY_ACTION);
        showSheet(closeSecondaryModel(() -> sheetClicks[0]++, CloseButtonVisibility.DEFAULT));
        assertEquals(
                View.VISIBLE, sheetButton(R.id.adaptive_ui_sheet_secondary_button).getVisibility());
        assertEquals(View.GONE, sheetButton(R.id.adaptive_ui_sheet_close_button).getVisibility());
        clickSheet(R.id.adaptive_ui_sheet_secondary_button);
        sheetSecondaryWatcher.assertExpected();
        assertEquals(0, sheetClicks[0]);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDismissReasons);
        mDismissReasons.clear();

        HistogramWatcher sheetCloseWatcher =
                watchDismissReason(/* isDialog= */ false, DismissReason.CLOSE_BUTTON);
        showSheet(closeSecondaryModel(() -> sheetClicks[0]++, CloseButtonVisibility.SHOWN));
        assertEquals(
                View.VISIBLE, sheetButton(R.id.adaptive_ui_sheet_close_button).getVisibility());
        assertEquals(
                View.VISIBLE, sheetButton(R.id.adaptive_ui_sheet_secondary_button).getVisibility());
        clickSheet(R.id.adaptive_ui_sheet_close_button);
        sheetCloseWatcher.assertExpected();
        assertEquals(0, sheetClicks[0]);
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDismissReasons);
        mDismissReasons.clear();

        int[] dialogClicks = new int[1];
        for (int scope : new int[] {AdaptiveUiScope.TRANSIENT, AdaptiveUiScope.WINDOW}) {
            // Exercise both CloseButtonVisibility.DEFAULT and CloseButtonVisibility.SHOWN; dialogs
            // show 'X' for both.
            @CloseButtonVisibility
            int closeButtonVisibility =
                    scope == AdaptiveUiScope.WINDOW
                            ? CloseButtonVisibility.SHOWN
                            : CloseButtonVisibility.DEFAULT;
            HistogramWatcher dialogWatcher =
                    watchDismissReason(/* isDialog= */ true, DismissReason.CLOSE_BUTTON);
            showDialog(closeSecondaryModel(() -> dialogClicks[0]++, closeButtonVisibility), scope);
            assertEquals(
                    scope == AdaptiveUiScope.TRANSIENT ? ModalDialogType.TAB : ModalDialogType.APP,
                    mDialogManager.getCurrentType());
            assertEquals(View.GONE, dialogButton(ButtonType.NEGATIVE).getVisibility());
            assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
            dialogCloseButton().performClick();
            dialogWatcher.assertExpected();
        }
        assertEquals(0, dialogClicks[0]);
        assertEquals(
                List.of(DismissReason.CLOSE_BUTTON, DismissReason.CLOSE_BUTTON), mDismissReasons);
    }

    @Test
    public void testCloseSecondaryBehavior_HiddenCloseButtonDrawsNegativeOnDialog() {
        int[] clicks = new int[1];
        HistogramWatcher secondaryWatcher =
                watchDismissReason(/* isDialog= */ true, DismissReason.SECONDARY_ACTION);
        showDialog(closeSecondaryModel(() -> clicks[0]++, CloseButtonVisibility.HIDDEN));
        assertEquals(View.GONE, dialogCloseButton().getVisibility());
        assertEquals(View.VISIBLE, dialogButton(ButtonType.NEGATIVE).getVisibility());
        dialogButton(ButtonType.NEGATIVE).performClick();
        secondaryWatcher.assertExpected();
        assertEquals(0, clicks[0]);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDismissReasons);
    }

    @Test
    public void testCloseSecondaryBehavior_SecurityFilterWithoutPrimaryShowsOnDialog() {
        int[] clicks = new int[1];
        PropertyModel filterTouchModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.CLOSE)
                        .with(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        HistogramWatcher closeWatcher =
                watchDismissReason(/* isDialog= */ true, DismissReason.CLOSE_BUTTON);
        showDialog(filterTouchModel);
        assertEquals(View.GONE, dialogButton(ButtonType.POSITIVE).getVisibility());
        assertEquals(View.GONE, dialogButton(ButtonType.NEGATIVE).getVisibility());
        assertEquals(View.VISIBLE, dialogCloseButton().getVisibility());
        dialogCloseButton().performClick();
        closeWatcher.assertExpected();
        assertEquals(0, clicks[0]);
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDismissReasons);
    }

    @Test
    public void testCloseSecondaryBehavior_NonButtonDismissalsSkipCallback() {
        int[] clicks = new int[1];
        showDialog(
                closeSecondaryModel(() -> clicks[0]++, CloseButtonVisibility.DEFAULT),
                AdaptiveUiScope.TRANSIENT);
        assertTrue(
                dialogView()
                        .dispatchKeyEvent(
                                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE)));

        showDialog(
                closeSecondaryModel(() -> clicks[0]++, CloseButtonVisibility.DEFAULT),
                AdaptiveUiScope.TRANSIENT);
        assertTrue(mTabDialogPresenter.getDialogContainerForTest().performClick());

        showDialog(
                closeSecondaryModel(() -> clicks[0]++, CloseButtonVisibility.DEFAULT),
                AdaptiveUiScope.WINDOW);
        mDialogPresenter.getDialogForTesting().getOnBackPressedDispatcher().onBackPressed();

        showDialog(
                closeSecondaryModel(() -> clicks[0]++, CloseButtonVisibility.DEFAULT),
                AdaptiveUiScope.WINDOW);
        mDialogPresenter.getDialogForTesting().cancel();
        ShadowLooper.idleMainLooper();

        assertEquals(0, clicks[0]);
        assertEquals(
                List.of(
                        DismissReason.BACK_PRESS,
                        DismissReason.TOUCH_OUTSIDE,
                        DismissReason.BACK_PRESS,
                        DismissReason.TOUCH_OUTSIDE),
                mDismissReasons);
    }

    private void verifyObscuredTouchFiltering(boolean isSheet) {
        int[] primaryClicks = new int[1];
        int[] secondaryClicks = new int[1];
        var model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.ok))
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                        .with(
                                AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                () -> secondaryClicks[0]++)
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN)
                        .with(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .build();
        if (isSheet) {
            showSheet(model);
        } else {
            showDialog(model);
        }
        View primary =
                isSheet
                        ? sheetButton(R.id.adaptive_ui_sheet_primary_button)
                        : dialogButton(ButtonType.POSITIVE);
        View secondary =
                isSheet
                        ? sheetButton(R.id.adaptive_ui_sheet_secondary_button)
                        : dialogButton(ButtonType.NEGATIVE);
        dispatchTap(primary, MotionEvent.FLAG_WINDOW_IS_OBSCURED);
        dispatchTap(secondary, MotionEvent.FLAG_WINDOW_IS_OBSCURED);
        dispatchTap(primary, MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        dispatchTap(secondary, MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        assertEquals(0, primaryClicks[0]);
        assertEquals(0, secondaryClicks[0]);
        dispatchTap(secondary, /* flags= */ 0);
        assertEquals(1, secondaryClicks[0]);
        assertTrue(mDismissReasons.isEmpty());
        dispatchTap(primary, /* flags= */ 0);
        if (isSheet) mSheetSupport.endAllAnimations();
        assertEquals(1, primaryClicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissReasons);
    }

    private static HistogramWatcher watchDismissReason(
            boolean isDialog, @DismissReason int reason) {
        String suffix = isDialog ? "ModalDialog" : "BottomSheet";
        return HistogramWatcher.newBuilder()
                .expectIntRecord("Android.AdaptiveUi.DismissReason." + suffix, reason)
                .build();
    }

    private @Nullable Void onScrim(PropertyModel params, boolean visible) {
        mScrimModel = visible ? params : null;
        params.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(visible);
        return null;
    }

    private String str(int resId) {
        return mActivity.getResources().getString(resId);
    }

    /** Every model records its lifecycle callbacks in the test's lists. */
    private PropertyModel.Builder newModelBuilder() {
        return new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS)
                .with(AdaptiveUiProperties.ON_SHOWN, mShownContainers::add)
                .with(AdaptiveUiProperties.ON_DISMISSED, mDismissReasons::add);
    }

    private PropertyModel primaryModel(Runnable onPrimary, long tapProtectionMs) {
        return newModelBuilder()
                .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.ok))
                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, onPrimary)
                .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, tapProtectionMs)
                .build();
    }

    private PropertyModel secondaryModel(Runnable onSecondary) {
        return newModelBuilder()
                .with(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION, str(R.string.cancel))
                .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, onSecondary)
                .build();
    }

    private PropertyModel closeSecondaryModel(
            Runnable onSecondary, @CloseButtonVisibility int closeButtonVisibility) {
        return newModelBuilder()
                .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                .with(AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR, SecondaryButtonBehavior.CLOSE)
                .with(AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY, closeButtonVisibility)
                .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, onSecondary)
                .build();
    }

    private PropertyModel tallSheetModel(boolean gestureDismissalDisabled) {
        View customView = new View(mActivity);
        customView.setMinimumHeight(TALL_CUSTOM_VIEW_HEIGHT_PX);
        return newModelBuilder()
                .with(AdaptiveUiProperties.TITLE, str(R.string.ok))
                .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                .with(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, gestureDismissalDisabled)
                .build();
    }

    private AdaptiveUiEntry newEntry(
            PropertyModel model, @AdaptiveUiScope int scope, @AdaptiveUiContainer int container) {
        return new AdaptiveUiEntry.Builder(model)
                .setScope(scope)
                .addAllowedContainer(container)
                .build();
    }

    private AdaptiveUiEntry newHalfSheetEntry() {
        var model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .with(AdaptiveUiProperties.SHEET_INITIAL_STATE, SheetState.HALF)
                        .build();
        return new AdaptiveUiEntry.Builder(model)
                .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                .build();
    }

    private static OnBackPressedCallback backHandler(int[] count) {
        return new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                count[0]++;
            }
        };
    }

    private TestBottomSheetContent otherSheet(@UserCriticalFeature int critical) {
        var sheet = new TestBottomSheetContent(mActivity, ContentPriority.HIGH, false);
        sheet.setPeekHeight(HeightMode.DISABLED);
        var type = new BottomSheetType.Builder().setModal(true).setUserInitiated(true);
        sheet.setSheetType(type.setUserCritical(critical).build());
        return sheet;
    }

    private void layoutRoot() {
        mActivity.getWindow().getDecorView().layout(0, 0, ROOT_SIZE_PX, ROOT_SIZE_PX);
        int spec = MeasureSpec.makeMeasureSpec(ROOT_SIZE_PX, MeasureSpec.EXACTLY);
        mTabDialogContainer.measure(spec, spec);
        mTabDialogContainer.layout(0, 0, ROOT_SIZE_PX, ROOT_SIZE_PX);
        mSheetContainer.measure(spec, spec);
        mSheetContainer.layout(0, 0, ROOT_SIZE_PX, ROOT_SIZE_PX);
    }

    /** Shows a sheet whose body is a scroll view with far more content than fits. */
    private ScrollView showSheetWithSelfScrollingBody() {
        ScrollView body = new ScrollView(mActivity);
        View tallContent = new View(mActivity);
        tallContent.setMinimumHeight(10 * ROOT_SIZE_PX);
        body.addView(tallContent);
        var model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, body)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                        .build();
        showSheet(model);
        return body;
    }

    private AdaptiveUiEntry showSheet(PropertyModel model) {
        var entry = newEntry(model, AdaptiveUiScope.TRANSIENT, AdaptiveUiContainer.BOTTOM_SHEET);
        assertEquals(Integer.valueOf(AdaptiveUiContainer.BOTTOM_SHEET), mController.show(entry));
        settleSheet();
        return entry;
    }

    // Robolectric runs no automatic layout pass, and FakeTimeTestRule does not drive animators.
    private void settleSheet() {
        layoutRoot();
        mSheetSupport.endAllAnimations();
        layoutRoot();
    }

    // TRANSIENT dialogs use TestTabModalPresenter (ModalDialogType.TAB); WINDOW dialogs use
    // AppModalPresenter (ModalDialogType.APP).
    private AdaptiveUiEntry showDialog(PropertyModel model) {
        return showDialog(model, AdaptiveUiScope.TRANSIENT);
    }

    private AdaptiveUiEntry showDialog(PropertyModel model, @AdaptiveUiScope int scope) {
        var entry = newEntry(model, scope, AdaptiveUiContainer.MODAL_DIALOG);
        assertEquals(Integer.valueOf(AdaptiveUiContainer.MODAL_DIALOG), mController.show(entry));
        // Drain the main looper so the presenter finishes showing (AppModalPresenter starts tap
        // protection in its OnShowListener).
        ShadowLooper.idleMainLooper();
        layoutRoot();
        return entry;
    }

    private void clickSheet(int id) {
        sheetButton(id).performClick();
        mSheetSupport.endAllAnimations();
    }

    private View sheetContainerView() {
        return mSheetSupport.getSheetContainer().findViewById(R.id.bottom_sheet);
    }

    private View sheetButton(int id) {
        return mSheetController.getCurrentSheetContent().getContentView().findViewById(id);
    }

    /** Returns the bottom edge of {@code view} relative to the top of the sheet content view. */
    private int bottomInSheetContent(View view) {
        View contentView = mSheetController.getCurrentSheetContent().getContentView();
        int bottom = view.getBottom();
        for (View parent = (View) view.getParent();
                parent != contentView;
                parent = (View) parent.getParent()) {
            bottom += parent.getTop();
        }
        return bottom;
    }

    private ModalDialogView dialogView() {
        ModalDialogView view =
                mDialogManager.getCurrentType() == ModalDialogType.TAB
                        ? mTabDialogPresenter.getDialogViewForTest()
                        : mDialogPresenter.getDialogViewForTesting();
        assertNotNull(view);
        return view;
    }

    private View dialogButton(@ButtonType int buttonType) {
        return dialogView().findViewWithTag(ModalDialogView.getTagForButtonType(buttonType));
    }

    private View dialogCloseButton() {
        return dialogView().findViewById(R.id.title_close_button);
    }

    private static void dispatchTap(View target, int flags) {
        long t = TimeUtils.uptimeMillis();
        var props = new MotionEvent.PointerProperties[] {new MotionEvent.PointerProperties()};
        var coords = new MotionEvent.PointerCoords[] {new MotionEvent.PointerCoords()};
        coords[0].x = target.getWidth() / 2f;
        coords[0].y = target.getHeight() / 2f;
        for (int action : new int[] {MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
            MotionEvent ev =
                    MotionEvent.obtain(
                            /* downTime= */ t,
                            /* eventTime= */ t,
                            action,
                            /* pointerCount= */ 1,
                            props,
                            coords,
                            /* metaState= */ 0,
                            /* buttonState= */ 0,
                            /* xPrecision= */ 1f,
                            /* yPrecision= */ 1f,
                            /* deviceId= */ 0,
                            /* edgeFlags= */ 0,
                            InputDevice.SOURCE_CLASS_POINTER,
                            flags);
            target.dispatchTouchEvent(ev);
            ev.recycle();
        }
        ShadowLooper.idleMainLooper();
    }

    private void dispatchVerticalSwipe(View sheetView) {
        dispatchVerticalSwipe(sheetView, /* distanceFraction= */ 1f, SWIPE_STEPS, SWIPE_STEP_MS);
    }

    // Drags 15% of the sheet height (< 30% state threshold) fast enough that fling velocity
    // carries the projected offset past 30% while remaining > 0px.
    private void dispatchShortFastDownwardFlick(View sheetView) {
        dispatchVerticalSwipe(sheetView, FLICK_DISTANCE_FRACTION, FLICK_STEPS, FLICK_STEP_MS);
    }

    private void dispatchVerticalSwipe(
            View sheetView, float distanceFraction, int steps, long stepMs) {
        long downTime = TimeUtils.uptimeMillis();
        float x = sheetView.getWidth() / 2f;
        float startY = sheetView.getHeight() - mSheetSupport.getCurrentOffsetPx() / 2f;
        float endY = startY + mSheetSupport.getCurrentOffsetPx() * distanceFraction;
        for (int i = 0; i <= steps + 1; i++) {
            int action = MotionEvent.ACTION_MOVE;
            if (i == 0) action = MotionEvent.ACTION_DOWN;
            if (i > steps) action = MotionEvent.ACTION_UP;
            float y = startY + (endY - startY) * Math.min(i, steps) / steps;
            var ev =
                    MotionEvent.obtain(
                            downTime, downTime + i * stepMs, action, x, y, /* metaState= */ 0);
            sheetView.dispatchTouchEvent(ev);
            ev.recycle();
        }
    }

    private boolean dispatchDialogOutsideTouch() {
        long t = TimeUtils.uptimeMillis();
        MotionEvent ev =
                MotionEvent.obtain(
                        /* downTime= */ t,
                        /* eventTime= */ t,
                        MotionEvent.ACTION_OUTSIDE,
                        /* x= */ 0f,
                        /* y= */ 0f,
                        /* metaState= */ 0);
        boolean handled = mDialogPresenter.getDialogForTesting().onTouchEvent(ev);
        ev.recycle();
        ShadowLooper.idleMainLooper();
        return handled;
    }
}

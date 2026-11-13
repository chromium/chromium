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
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

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
import org.chromium.build.annotations.Nullable;
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
public class AdaptiveUiIntegrationTest implements AdaptiveUiObserver {
    private static final int ROOT_SIZE_PX = 2000;
    private static final long PERIOD_MS = 500;
    private static final long LONGER_THAN_PERIOD_MS = PERIOD_MS * 2;
    private static final int SWIPE_STEPS = 10;
    private static final long SWIPE_STEP_MS = 10;

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
    private FrameLayout mSheetContainer;
    private ManagedBottomSheetController mSheetController;
    private BottomSheetTestSupport mSheetSupport;
    private AppModalPresenter mDialogPresenter;
    private ModalDialogManager mDialogManager;
    private AdaptiveUiControllerImpl mController;
    private @Nullable PropertyModel mScrimModel;

    @Override
    public void onShown(@AdaptiveUiContainer int container) {
        mShownContainers.add(container);
    }

    @Override
    public void onDismissed(@DismissReason int reason) {
        mDismissReasons.add(reason);
    }

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mSheetContainer = new FrameLayout(mActivity);
        mActivity.setContentView(mSheetContainer);
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
        mDialogManager = new ModalDialogManager(mDialogPresenter, ModalDialogType.APP);
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
        clickSheet(R.id.adaptive_ui_sheet_secondary_button);
        assertEquals(1, secondaryClicks[0]);
        showDialog(secondaryModel(() -> secondaryClicks[0]++));
        dialogButton(ButtonType.NEGATIVE).performClick();
        assertEquals(2, secondaryClicks[0]);
        assertEquals(
                List.of(DismissReason.SECONDARY_ACTION, DismissReason.SECONDARY_ACTION),
                mDismissReasons);
        mDismissReasons.clear();

        showSheet(newModelBuilder().with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true).build());
        clickSheet(R.id.adaptive_ui_sheet_close_button);
        showDialog(newModelBuilder().with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true).build());
        dialogCloseButton().performClick();
        assertEquals(
                List.of(DismissReason.CLOSE_BUTTON, DismissReason.CLOSE_BUTTON), mDismissReasons);
    }

    @Test
    public void testControllerDestroy_ClosesOpenEntriesAndDetaches() {
        showSheet(newModelBuilder().build());
        showDialog(newModelBuilder().build());
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

    private void verifyObscuredTouchFiltering(boolean isSheet) {
        int[] primaryClicks = new int[1];
        int[] secondaryClicks = new int[1];
        var model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                        .with(
                                AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                () -> secondaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN, true)
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

    private @Nullable Void onScrim(PropertyModel params, boolean visible) {
        mScrimModel = visible ? params : null;
        params.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(visible);
        return null;
    }

    private String str(int resId) {
        return mActivity.getResources().getString(resId);
    }

    private static PropertyModel.Builder newModelBuilder() {
        return new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS);
    }

    private PropertyModel primaryModel(Runnable onPrimary, long tapProtectionMs) {
        return newModelBuilder()
                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, str(R.string.ok))
                .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, onPrimary)
                .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, tapProtectionMs)
                .build();
    }

    private PropertyModel secondaryModel(Runnable onSecondary) {
        return newModelBuilder()
                .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, str(R.string.cancel))
                .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, onSecondary)
                .build();
    }

    private AdaptiveUiEntry newEntry(
            PropertyModel model, @AdaptiveUiScope int scope, @AdaptiveUiContainer int container) {
        return new AdaptiveUiEntry.Builder(model)
                .setScope(scope)
                .addAllowedContainer(container)
                .setObserver(this)
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
        mSheetContainer.measure(spec, spec);
        mSheetContainer.layout(0, 0, ROOT_SIZE_PX, ROOT_SIZE_PX);
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

    // TRANSIENT dialogs request TAB modality and fall back to ModalDialogManager's default
    // AppModalPresenter here; assertions depend only on shared ModalDialogView behavior.
    private void showDialog(PropertyModel model) {
        showDialog(model, AdaptiveUiScope.TRANSIENT);
    }

    private void showDialog(PropertyModel model, @AdaptiveUiScope int scope) {
        var entry = newEntry(model, scope, AdaptiveUiContainer.MODAL_DIALOG);
        assertEquals(Integer.valueOf(AdaptiveUiContainer.MODAL_DIALOG), mController.show(entry));
        // Drain Dialog's SHOW message so AppModalPresenter's OnShowListener starts tap protection.
        ShadowLooper.idleMainLooper();
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

    private View dialogButton(@ButtonType int buttonType) {
        return mDialogPresenter
                .getDialogViewForTesting()
                .findViewWithTag(ModalDialogView.getTagForButtonType(buttonType));
    }

    private View dialogCloseButton() {
        return mDialogPresenter.getDialogViewForTesting().findViewById(R.id.title_close_button);
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
        long downTime = TimeUtils.uptimeMillis();
        float x = sheetView.getWidth() / 2f;
        float startY = sheetView.getHeight() - mSheetSupport.getCurrentOffsetPx() / 2f;
        float endY = startY + mSheetSupport.getCurrentOffsetPx();
        for (int i = 0; i <= SWIPE_STEPS + 1; i++) {
            int action = MotionEvent.ACTION_MOVE;
            if (i == 0) action = MotionEvent.ACTION_DOWN;
            if (i > SWIPE_STEPS) action = MotionEvent.ACTION_UP;
            float y = startY + (endY - startY) * Math.min(i, SWIPE_STEPS) / SWIPE_STEPS;
            var ev = MotionEvent.obtain(downTime, downTime + i * SWIPE_STEP_MS, action, x, y, 0);
            sheetView.dispatchTouchEvent(ev);
            ev.recycle();
        }
    }
}

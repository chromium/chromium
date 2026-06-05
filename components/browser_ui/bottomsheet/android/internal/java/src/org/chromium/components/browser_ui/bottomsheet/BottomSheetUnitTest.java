// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheet.ShadowLayerView;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.util.ColorUtils;

/** Unit tests for {@link BottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;
    private static final int SHEET_CONTAINER_HEIGHT = 200;
    private static final int SHEET_CONTAINER_WIDTH = 1080;
    private static final int SHEET_PEEK_HEIGHT = 60;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mSheetBackground;
    @Mock private ShadowLayerView mShadowLayerView;
    @Mock private BottomSheetContent mSheetContent;
    @Mock private TouchRestrictingFrameLayout mToolbarHolder;
    @Mock private InsetObserver mInsetObserver;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private BottomSheetObserver mBottomSheetObserver;

    @Captor
    private ArgumentCaptor<InsetObserver.WindowInsetsAnimationListener>
            mInsetsAnimationListenerCaptor;

    @Captor private ArgumentCaptor<KeyboardVisibilityListener> mKeyboardListenerCaptor;
    @Captor private ArgumentCaptor<InsetObserver.WindowInsetObserver> mInsetObserverCaptor;

    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private BottomSheet mBottomSheet;
    private ViewGroup mSheetContainer;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomSheet =
                (BottomSheet) LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet, null);

        FrameLayout sheetContainerParent = new FrameLayout(mActivity);
        mSheetContainer = new FrameLayout(mActivity);
        mSheetContainer.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, SHEET_CONTAINER_HEIGHT));

        sheetContainerParent.addView(mSheetContainer);
        mSheetContainer.addView(mBottomSheet);

        // Measure and layout the container so it has a size.
        mSheetContainer.measure(
                View.MeasureSpec.makeMeasureSpec(SHEET_CONTAINER_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(SHEET_CONTAINER_HEIGHT, View.MeasureSpec.EXACTLY));
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        mBottomSheet.setSheetContainerForTesting(mSheetContainer);
        mBottomSheet.setToolbarHolderForTesting(mToolbarHolder);
        mBottomSheet.setBottomSheetContentContainerForTesting(
                mBottomSheet.findViewById(R.id.bottom_sheet_content));

        mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
        doReturn(mKeyboardInsetSupplier).when(mInsetObserver).getSupplierForKeyboardInset();

        mBottomSheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver);

        mBottomSheet.setSheetBackgroundForTesting(mSheetBackground);
        mBottomSheet.setShadowLayerForTesting(mShadowLayerView);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchToDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        assertEquals(
                "Sheet container's top margin should be updated to account for app header height.",
                APP_HEADER_HEIGHT,
                params.topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchOutOfDesktopWindow() {
        // Sheet container in desktop window will use top margin = APP_HEADER_HEIGHT.
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        params.topMargin = APP_HEADER_HEIGHT;
        mSheetContainer.setLayoutParams(params);

        // Simulate switching out of desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(0);

        params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        assertEquals("Sheet container's top margin should be reset.", 0, params.topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SameAsContainerTopMargin() {
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        params.topMargin = APP_HEADER_HEIGHT;
        mSheetContainer.setLayoutParams(params);

        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);

        assertEquals(
                "Sheet container's top margin should be unchanged.",
                APP_HEADER_HEIGHT,
                ((MarginLayoutParams) mSheetContainer.getLayoutParams()).topMargin);
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtFullHeightScrimmed_Dark() {
        doTestBackgroundColorFullHeightScrimmed(
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity));
    }

    @Test
    public void testBackgroundColorAtFullHeightScrimmed_Light() {
        doTestBackgroundColorFullHeightScrimmed(SemanticColorUtils.getColorSurface(mActivity));
    }

    private void doTestBackgroundColorFullHeightScrimmed(int expectedColor) {
        doReturn(HeightMode.DISABLED).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(false).when(mSheetContent).hasCustomScrimLifecycle();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Scrimmed sheet bg color is wrong.",
                expectedColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtFullHeightUnscrimmed_Dark() {
        showFullHeightUnscrimmedSheetAndVerifyBackgroundColor();
    }

    @Test
    public void testBackgroundColorAtFullHeightUnscrimmed_Light() {
        showFullHeightUnscrimmedSheetAndVerifyBackgroundColor();
    }

    private void showFullHeightUnscrimmedSheetAndVerifyBackgroundColor() {
        doReturn(HeightMode.DISABLED).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Unscrimmed sheet bg color is wrong.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testBackgroundColorAtPeekHeight_Light() {
        showSheetAtPeekHeightAndVerifyBackgroundColor();
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtPeekHeight_Dark() {
        showSheetAtPeekHeightAndVerifyBackgroundColor();
    }

    private void showSheetAtPeekHeightAndVerifyBackgroundColor() {
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "PEEK state sheet bg color is wrong.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorTransitionInDark() {
        int offset = 130;
        int expectedColor =
                ColorUtils.overlayColor(
                        SemanticColorUtils.getColorSurface(mActivity),
                        SemanticColorUtils.getColorSurfaceContainerHigh(mActivity),
                        0.5f // fraction = (130 - 60) / (200 - 60) = 0.5
                        );
        showSheetThenScrollToHalfOffsetAndVerifyColor(offset, expectedColor);
    }

    @Test
    public void testBackgroundColorNoTransitionInLight() {
        // Sheet color does not transition in light mode.
        showSheetThenScrollToHalfOffsetAndVerifyColor(
                130, SemanticColorUtils.getColorSurface(mActivity));
    }

    private void showSheetThenScrollToHalfOffsetAndVerifyColor(int offset, int expectedColor) {
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetOffset(offset, false);

        assertEquals(
                "Half-height state sheet bg is different.",
                expectedColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testBackgroundColorOverride() {
        final int overrideColor = Color.CYAN;
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(overrideColor).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Sheet bg color should be the override color.",
                overrideColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testGetFullRatio_ResizeContent() {
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Full ratio for RESIZE_CONTENT should be MAX_HEIGHT_RATIO.",
                1.0f,
                mBottomSheet.getFullRatio(),
                0.0f);
    }

    @Test
    public void testGetFullRatio_ResizeContent_HalfStateDisabled() {
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Full ratio for RESIZE_CONTENT with half state disabled should be 1.0f.",
                1.0f,
                mBottomSheet.getFullRatio(),
                0.0f);
    }

    @Test
    public void testSetSheetOffsetFromBottom_ResizeContent() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();

        // Return 0.5 for half height to make the min height 100 (container height is 200)
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(R.string.bottom_sheet_accessibility_description)
                .when(mSheetContent)
                .getSheetClosedAccessibilityStringId();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be max(100, 150) = 150.
        assertEquals(150, contentContainer.getLayoutParams().height);

        mBottomSheet.setSheetOffsetFromBottom(50.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be max(100, 50) = 100.
        assertEquals(100, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testOnSheetContentChanged_ResizeContentRestore() {
        BottomSheet.setSmallScreenForTesting(false);

        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(R.string.bottom_sheet_accessibility_description)
                .when(mSheetContent)
                .getSheetClosedAccessibilityStringId();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);
        assertEquals(150, contentContainer.getLayoutParams().height);

        // Hide content (which means content == null in onSheetContentChanged)
        mBottomSheet.showContent(null);
        // The container's layout params height should be restored to MATCH_PARENT
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testBackgroundColorOverride_Transparent() {
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(Color.TRANSPARENT).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Sheet bg color should be the override color.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testKeyboardCurtainColor_BackgroundColorOverride() {
        final int overrideColor = Color.CYAN;
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(overrideColor).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);

        View curtain = mBottomSheet.findViewById(R.id.keyboard_curtain);
        assertEquals(
                "Keyboard curtain bg color should be the override color.",
                ColorStateList.valueOf(overrideColor),
                curtain.getBackgroundTintList());
    }

    @Test
    public void testBackgroundGlowColor() {
        final int glowColor = Color.CYAN;
        BottomSheetContent.GlowSpec spec =
                new BottomSheetContent.GlowSpec(
                        glowColor, BottomSheetContent.GlowSpec.ShadowSize.LONG);
        doReturn(spec).when(mSheetContent).getSheetBackgroundGlowSpecOverride();

        mBottomSheet.showContent(mSheetContent);

        verify(mShadowLayerView).setBackgroundTintList(ColorStateList.valueOf(glowColor));
        int expectedSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length_large);
        verify(mShadowLayerView).setShadowLength(expectedSize);
    }

    @Test
    public void testBackgroundGlowColor_Transparent() {
        doReturn(null).when(mSheetContent).getSheetBackgroundGlowSpecOverride();

        mBottomSheet.showContent(mSheetContent);

        verify(mShadowLayerView).setBackgroundTintList(null);
        int defaultSize =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
        verify(mShadowLayerView).setShadowLength(defaultSize);
    }

    @Test
    public void testWindowInsetsAnimationListener() {
        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mActivity.getWindow().getDecorView().layout(0, 0, 1080, 200);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        WindowInsetsCompat insets = mock(WindowInsetsCompat.class);
        doReturn(insets).when(mInsetObserver).getLastRawWindowInsets();

        // Full viewport.
        doReturn(Insets.of(0, 0, 0, 0)).when(insets).getInsets(anyInt());

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        assertEquals(150, contentContainer.getLayoutParams().height);

        // Simulate keyboard showing up, shrinking viewport to 100.
        doReturn(Insets.of(0, 0, 0, 100)).when(insets).getInsets(anyInt());

        listener.onStart(null, null);
        assertEquals(100, contentContainer.getLayoutParams().height);

        // Viewport shrinks to 50.
        doReturn(Insets.of(0, 0, 0, 150)).when(insets).getInsets(anyInt());

        listener.onProgress(null, null);
        assertEquals(50, contentContainer.getLayoutParams().height);

        // Viewport shrinks to 20.
        doReturn(Insets.of(0, 0, 0, 180)).when(insets).getInsets(anyInt());

        listener.onEnd(null);
        assertEquals(20, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testRevertStateOnKeyboardHiding() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetHalfHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetFullHeightAccessibilityStringId();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.HALF, false);

        // Simulate keyboard showing
        verify(mInsetObserver).addObserver(mInsetObserverCaptor.capture());
        InsetObserver.WindowInsetObserver observer = mInsetObserverCaptor.getValue();

        mKeyboardInsetSupplier.set(100);
        observer.onInsetChanged();

        // Simulate layout change while keyboard is showing (Pass 1)
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Simulate keyboard hiding.
        mKeyboardInsetSupplier.set(0);
        observer.onInsetChanged();
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Verify that state is restored to HALF.
        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());
    }

    @Test
    public void testCancelRevertStateDueToHeightChange() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetHalfHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetFullHeightAccessibilityStringId();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.HALF, false);

        // Set initial decor view size and trigger layout to set mPreviousScreenHeight
        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1920);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Simulate keyboard showing
        verify(mInsetObserver).addObserver(mInsetObserverCaptor.capture());
        InsetObserver.WindowInsetObserver observer = mInsetObserverCaptor.getValue();

        mKeyboardInsetSupplier.set(100);
        observer.onInsetChanged();

        // Simulate layout change while keyboard is showing
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Change state while keyboard is showing
        mBottomSheet.setSheetState(SheetState.FULL, false);

        // Simulate screen height change
        decorView.layout(0, 0, 1080, 1820);

        // Simulate keyboard hiding.
        mKeyboardInsetSupplier.set(0);
        observer.onInsetChanged();

        // Trigger layout change on container
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Verify that state is NOT restored to HALF, but stays FULL.
        assertEquals(SheetState.FULL, mBottomSheet.getSheetState());
    }

    @Test
    public void testRecreateStateOnKeyboardShowingWithHeightChange() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetHalfHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetFullHeightAccessibilityStringId();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.HALF, false);

        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1920);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        verify(mInsetObserver).addObserver(mInsetObserverCaptor.capture());
        InsetObserver.WindowInsetObserver observer = mInsetObserverCaptor.getValue();
        mKeyboardInsetSupplier.set(100);
        observer.onInsetChanged();

        // Simulate layout change while keyboard is showing.
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Simulate screen height change.
        decorView.layout(0, 0, 1080, 1820);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        mKeyboardInsetSupplier.set(150);
        observer.onInsetChanged();
    }

    @Test
    public void triggerObserverOnInsetChange() {
        mBottomSheet.addObserver(mBottomSheetObserver);
        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        listener.onPrepare(null);
        verify(mBottomSheetObserver).beforeInsetAnimationStart();

        listener.onEnd(null);
        verify(mBottomSheetObserver).onInsetAnimationEnd();
    }

    @Test
    public void testUpdateA11yPaneTitle() {
        int stringId = android.R.string.ok;
        doReturn(stringId).when(mSheetContent).getSheetFullHeightAccessibilityStringId();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.FULL, false);

        CharSequence expectedTitle = mActivity.getResources().getString(stringId);
        assertEquals(
                "Accessibility pane title should be set on the BottomSheet itself",
                expectedTitle,
                androidx.core.view.ViewCompat.getAccessibilityPaneTitle(mBottomSheet));
    }
}

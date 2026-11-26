// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.floating_pill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.floating_pill.FloatingPillLayer.PillType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;

/** Unit tests for {@link FloatingPillLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FLOATING_PILL_LAYER)
public class FloatingPillLayerUnitTest {
    private static final int TOP_CONTROLS_HEIGHT = 100;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControls;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Captor private ArgumentCaptor<BrowserControlsStateProvider.Observer> mObserverCaptor;
    @Captor private ArgumentCaptor<SideUiObserver> mSideUiObserverCaptor;

    private final SettableNonNullObservableSupplier<Boolean> mOmniboxFocused =
            ObservableSuppliers.createNonNull(false);
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private Activity mActivity;
    private FrameLayout mParent;
    private FloatingPillLayer mLayer;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mParent = new FrameLayout(mActivity);
        ViewStub stub = new ViewStub(mActivity, R.layout.floating_pill_container);
        mParent.addView(
                stub,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        when(mBrowserControls.getTopControlsHeight()).thenReturn(TOP_CONTROLS_HEIGHT);
        when(mBrowserControls.getControlsPosition()).thenReturn(ControlsPosition.TOP);
        when(mSideUiStateProvider.getCurrentSideUiSpecs())
                .thenReturn(
                        new SideUiSpecs(
                                /* leftContainerWidth= */ 40, /* rightContainerWidth= */ 60));
        mLayer =
                new FloatingPillLayer(
                        stub,
                        mTopControlsStacker,
                        mBrowserControls,
                        mOmniboxFocused,
                        mSideUiStateProviderSupplier);
    }

    @Test
    public void testLayerProperties() {
        assertEquals(TopControlType.FLOATING_PILL, mLayer.getTopControlType());
        assertEquals(0, mLayer.getTopControlHeight());
        assertFalse(mLayer.contributesToTotalHeight());
    }

    @Test
    public void testFirstPillInflatesAndPlacesContainer() {
        assertNull(mLayer.getContainerForTesting());

        View pill = new View(mActivity);
        mLayer.addPill(pill, PillType.TTC_VOICE_PLATE);

        FloatingPillContainer container = getContainer();
        assertSame(container, pill.getParent());
        assertSame(mParent, container.getParent());
        assertEquals(View.VISIBLE, container.getVisibility());
        verify(mTopControlsStacker).addControl(mLayer);
        assertTopMargin(TOP_CONTROLS_HEIGHT, /* controlsAtBottom= */ false);
    }

    @Test
    public void testPillsFollowTypeOrder() {
        View ttc = new View(mActivity);
        View actor = new View(mActivity);
        mLayer.addPill(ttc, PillType.TTC_VOICE_PLATE);
        mLayer.addPill(actor, PillType.ACTOR_TAKE_OVER);

        FloatingPillContainer container = getContainer();
        assertSame(actor, container.getChildAt(0));
        assertSame(ttc, container.getChildAt(1));
        // Joins the stacker and observes the controls once, however many pills it has.
        verify(mTopControlsStacker).addControl(mLayer);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
    }

    @Test
    public void testPillsAreSpacedApart() {
        View actor = createPill();
        View ttc = createPill();
        mLayer.addPill(actor, PillType.ACTOR_TAKE_OVER);
        mLayer.addPill(ttc, PillType.TTC_VOICE_PLATE);

        layOut();
        int spacing = mActivity.getResources().getDimensionPixelSize(R.dimen.floating_pill_spacing);
        assertEquals(spacing, ttc.getLeft() - actor.getRight());

        // A hidden pill leaves no gap.
        actor.setVisibility(View.GONE);
        layOut();
        assertEquals(getContainer().getPaddingLeft(), ttc.getLeft());
    }

    @Test
    public void testFollowsTopControls() {
        mLayer.addPill(new View(mActivity), PillType.TTC_VOICE_PLATE);

        mLayer.onTopControlLayerHeightChanged(150, 0);
        assertTopMargin(150, /* controlsAtBottom= */ false);

        // In the stacker, the height only comes from onTopControlLayerHeightChanged().
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        mObserverCaptor.getValue().onTopControlsHeightChanged(200, 0);
        assertTopMargin(150, /* controlsAtBottom= */ false);

        when(mBrowserControls.getControlsPosition()).thenReturn(ControlsPosition.BOTTOM);
        mObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.BOTTOM);
        assertTopMargin(TOP_CONTROLS_HEIGHT, /* controlsAtBottom= */ true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FLOATING_PILL_LAYER)
    public void testWithoutFeatureStaysOutOfStacker() {
        ViewStub stub = new ViewStub(mActivity, R.layout.floating_pill_container);
        mParent.addView(
                stub,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        FloatingPillLayer layer =
                new FloatingPillLayer(
                        stub,
                        mTopControlsStacker,
                        mBrowserControls,
                        mOmniboxFocused,
                        mSideUiStateProviderSupplier);
        View pill = new View(mActivity);
        layer.addPill(pill, PillType.TTC_VOICE_PLATE);
        verify(mTopControlsStacker, never()).addControl(layer);

        // Follows the controls' height on its own.
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        mObserverCaptor.getValue().onTopControlsHeightChanged(150, 0);
        FloatingPillContainer container = layer.getContainerForTesting();
        assertNotNull(container);
        assertEquals(
                FloatingPillContainer.getTopMargin(
                        mActivity.getResources(), 150, /* controlsAtBottom= */ false),
                ((ViewGroup.MarginLayoutParams) container.getLayoutParams()).topMargin);

        layer.removePill(pill);
        verify(mTopControlsStacker, never()).removeControl(layer);
        verify(mBrowserControls).removeObserver(mObserverCaptor.getValue());
    }

    @Test
    public void testHiddenWhileOmniboxFocused() {
        mLayer.addPill(new View(mActivity), PillType.TTC_VOICE_PLATE);

        assertEquals(TopControlVisibility.VISIBLE, mLayer.getTopControlVisibility());

        mOmniboxFocused.set(true);
        assertEquals(View.GONE, getContainer().getVisibility());
        // The stacker sees it hidden too.
        assertEquals(TopControlVisibility.HIDDEN, mLayer.getTopControlVisibility());

        mOmniboxFocused.set(false);
        assertEquals(View.VISIBLE, getContainer().getVisibility());
        assertEquals(TopControlVisibility.VISIBLE, mLayer.getTopControlVisibility());
    }

    @Test
    public void testAvoidsSideUi() {
        setSideUiStateProvider();
        mLayer.addPill(new View(mActivity), PillType.TTC_VOICE_PLATE);
        assertSideMargins(40, 60);

        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());
        mSideUiObserverCaptor
                .getValue()
                .onSideUiSpecsChangeStarted(
                        new SideUiSpecs(/* leftContainerWidth= */ 0, /* rightContainerWidth= */ 80),
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        assertSideMargins(0, 80);
    }

    @Test
    public void testAvoidsSideUiThatComesLater() {
        mLayer.addPill(new View(mActivity), PillType.TTC_VOICE_PLATE);
        assertSideMargins(0, 0);

        setSideUiStateProvider();
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());
        assertSideMargins(40, 60);
    }

    @Test
    public void testLastPillLeavesStacker() {
        setSideUiStateProvider();
        View actor = new View(mActivity);
        View ttc = new View(mActivity);
        mLayer.addPill(actor, PillType.ACTOR_TAKE_OVER);
        mLayer.addPill(ttc, PillType.TTC_VOICE_PLATE);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        mLayer.removePill(actor);
        assertNull(actor.getParent());
        verify(mTopControlsStacker, never()).removeControl(mLayer);
        assertEquals(View.VISIBLE, getContainer().getVisibility());

        mLayer.removePill(ttc);
        verify(mTopControlsStacker).removeControl(mLayer);
        verify(mBrowserControls).removeObserver(mObserverCaptor.getValue());
        verify(mSideUiStateProvider).removeObserver(mSideUiObserverCaptor.getValue());
        assertEquals(View.GONE, getContainer().getVisibility());
        assertFalse(mOmniboxFocused.hasObservers());
    }

    @Test
    public void testAddAgainAfterLastPillRemoved() {
        setSideUiStateProvider();
        View pill = new View(mActivity);
        mLayer.addPill(pill, PillType.TTC_VOICE_PLATE);
        FloatingPillContainer container = getContainer();
        mLayer.removePill(pill);

        when(mSideUiStateProvider.getCurrentSideUiSpecs())
                .thenReturn(
                        new SideUiSpecs(
                                /* leftContainerWidth= */ 10, /* rightContainerWidth= */ 20));
        mLayer.addPill(pill, PillType.TTC_VOICE_PLATE);

        assertSame(container, getContainer());
        assertEquals(View.VISIBLE, container.getVisibility());
        verify(mTopControlsStacker, times(2)).addControl(mLayer);
        assertSideMargins(10, 20);
    }

    @Test
    public void testDestroyLeavesStacker() {
        setSideUiStateProvider();
        View pill = new View(mActivity);
        mLayer.addPill(pill, PillType.TTC_VOICE_PLATE);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        mLayer.destroy();

        assertNull(pill.getParent());
        verify(mTopControlsStacker).removeControl(mLayer);
        verify(mBrowserControls).removeObserver(mObserverCaptor.getValue());
        verify(mSideUiStateProvider).removeObserver(mSideUiObserverCaptor.getValue());
        assertFalse(mOmniboxFocused.hasObservers());
        View another = new View(mActivity);
        assertThrows(AssertionError.class, () -> mLayer.addPill(another, PillType.TTC_VOICE_PLATE));
    }

    private void setSideUiStateProvider() {
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        // The supplier posts the callback.
        ShadowLooper.idleMainLooper();
    }

    private View createPill() {
        View pill = new View(mActivity);
        pill.setMinimumWidth(100);
        pill.setMinimumHeight(56);
        return pill;
    }

    private void layOut() {
        mParent.measure(
                MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY));
        mParent.layout(0, 0, 1000, 1000);
    }

    private FloatingPillContainer getContainer() {
        FloatingPillContainer container = mLayer.getContainerForTesting();
        assertNotNull(container);
        return container;
    }

    private void assertTopMargin(int topControlsHeight, boolean controlsAtBottom) {
        assertEquals(
                FloatingPillContainer.getTopMargin(
                        mActivity.getResources(), topControlsHeight, controlsAtBottom),
                ((ViewGroup.MarginLayoutParams) getContainer().getLayoutParams()).topMargin);
    }

    private void assertSideMargins(int left, int right) {
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) getContainer().getLayoutParams();
        assertEquals(left, params.leftMargin);
        assertEquals(right, params.rightMargin);
    }
}

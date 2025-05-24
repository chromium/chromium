// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit test for {@link SimpleEdgeToEdgePadAdjuster}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SimpleEdgeToEdgePadAdjusterUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    private TestActivity mActivity;
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    @Test
    public void testRegularView() {
        View view = new View(mActivity);
        var padAdjuster = new SimpleEdgeToEdgePadAdjuster(view, true);

        int bottomInsets = 100;
        padAdjuster.overrideBottomInset(bottomInsets);
        assertEquals(bottomInsets, view.getPaddingBottom());

        padAdjuster.overrideBottomInset(0);
        assertEquals(0, view.getPaddingBottom());
    }

    @Test
    public void testScrollView() {
        doTestClipToPadding(new ScrollView(mActivity));
    }

    @Test
    public void testRecyclerView() {
        doTestClipToPadding(new RecyclerView(mActivity));
    }

    @Test
    public void testNonScrollingViewGroup() {
        FrameLayout frameLayout = new FrameLayout(mActivity);
        var padAdjuster = new SimpleEdgeToEdgePadAdjuster(frameLayout, true);
        doTestClipToPaddingUnchanged(frameLayout, padAdjuster);
    }

    @Test
    public void testDisableClipToPadding() {
        ScrollView scrollView = new ScrollView(mActivity);
        var padAdjuster = new SimpleEdgeToEdgePadAdjuster(scrollView, false);
        doTestClipToPaddingUnchanged(scrollView, padAdjuster);
    }

    private void doTestClipToPadding(ViewGroup viewGroup) {
        var padAdjuster = new SimpleEdgeToEdgePadAdjuster(viewGroup, true);

        int bottomInsets = 100;
        padAdjuster.overrideBottomInset(bottomInsets);
        assertEquals(bottomInsets, viewGroup.getPaddingBottom());
        assertFalse(
                "clipToPadding for RecyclerView when bottom inset > 0.",
                viewGroup.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals(0, viewGroup.getPaddingBottom());
        assertTrue("clipToPadding reset when bottom inset = 0.", viewGroup.getClipToPadding());
    }

    private void doTestClipToPaddingUnchanged(
            ViewGroup view, SimpleEdgeToEdgePadAdjuster padAdjuster) {
        int bottomInsets = 100;
        padAdjuster.overrideBottomInset(bottomInsets);
        assertEquals(bottomInsets, view.getPaddingBottom());
        assertTrue("clipToPadding should not change.", view.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals(0, view.getPaddingBottom());
        assertTrue("clipToPadding should not change.", view.getClipToPadding());
    }

    @Test
    public void testCreateForViewAndObserveSupplier() {
        View view = new View(mActivity);
        var padAdjuster =
                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                        view, mEdgeToEdgeControllerSupplier);

        assertNotNull(padAdjuster);
        assertTrue(
                "createForViewAndObserveSupplier will start observation.",
                mEdgeToEdgeControllerSupplier.hasObservers());

        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(padAdjuster);

        padAdjuster.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }

    @Test
    public void testDestroyResetPadding() {
        ScrollView view = new ScrollView(mActivity);

        int left = 1;
        int top = 2;
        int right = 3;
        int bottom = 4;
        view.setPadding(left, top, right, bottom);

        var padAdjuster = new SimpleEdgeToEdgePadAdjuster(view, true);

        int bottomInsets = 100;
        padAdjuster.overrideBottomInset(bottomInsets);
        assertEquals(left, view.getPaddingLeft());
        assertEquals(top, view.getPaddingTop());
        assertEquals(right, view.getPaddingRight());
        assertEquals(bottom + bottomInsets, view.getPaddingBottom());
        assertFalse(
                "clipToPadding should be set to false when insets > 0.", view.getClipToPadding());

        padAdjuster.destroy();
        assertEquals(left, view.getPaddingLeft());
        assertEquals(top, view.getPaddingTop());
        assertEquals(right, view.getPaddingRight());
        assertEquals(bottom, view.getPaddingBottom());
        assertTrue("clipToPadding should be reset to true.", view.getClipToPadding());
    }
}

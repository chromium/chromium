// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.appcompat.app.AlertDialog;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.edge_to_edge.layout.EdgeToEdgeBaseLayout;

/** Unit test for {@link FullscreenAlertDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class FullscreenAlertDialogUnitTest {

    private static final int TOP_INSETS = 48;
    private static final int BOTTOM_INSETS = 20;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    private Activity mActivity;

    @Test
    public void padForInsetsWithBuilder() {
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        AlertDialog dialog =
                new FullscreenAlertDialog.Builder(mActivity, true).setView(dialogView).create();

        assertWrappedWithEdgeToEdgeLayout(dialogView);
        verifyDialogApplyPaddingWithInsets(dialogView.getRootView());

        mActivity.finish();
    }

    @Test
    public void padForInsetsWithConstructor() {
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        FullscreenAlertDialog dialog = new FullscreenAlertDialog(mActivity, true);
        dialog.setView(dialogView);

        assertWrappedWithEdgeToEdgeLayout(dialogView);
        verifyDialogApplyPaddingWithInsets(dialogView.getRootView());

        mActivity.finish();
    }

    @Test
    public void notPadForInsetsWithBuilder() {
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        AlertDialog dialog =
                new FullscreenAlertDialog.Builder(mActivity, /* shouldPadForContent= */ false)
                        .setView(dialogView)
                        .create();
        dialog.show();

        Assert.assertFalse(
                "Dialog view should not be wrapped.",
                dialogView.getParent() instanceof EdgeToEdgeBaseLayout);
    }

    @Test
    public void notPadForInsetsWithConstructor() {
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        AlertDialog dialog = new FullscreenAlertDialog(mActivity, /* shouldPadForContent= */ false);
        dialog.setView(dialogView);
        dialog.show();

        Assert.assertFalse(
                "Dialog view should not be wrapped.",
                dialogView.getParent() instanceof EdgeToEdgeBaseLayout);
    }

    @Test
    public void padForInsetsDisabledOnAutomotive() {
        mAutomotiveRule.setIsAutomotive(true);
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        FullscreenAlertDialog dialog = new FullscreenAlertDialog(mActivity, true);
        dialog.setView(dialogView);

        assertWrappedWithAutomotiveLayout(dialogView);

        mActivity.finish();
    }

    @Test
    public void padForInsetsDisabledOnAutomotiveWithBuilder() {
        mAutomotiveRule.setIsAutomotive(true);
        launchActivity();

        FrameLayout dialogView = new FrameLayout(mActivity);
        AlertDialog dialog =
                new FullscreenAlertDialog.Builder(mActivity, true).setView(dialogView).create();

        assertWrappedWithAutomotiveLayout(dialogView);

        mActivity.finish();
    }

    private void verifyDialogApplyPaddingWithInsets(View dialogRootView) {
        WindowInsetsCompat windowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, TOP_INSETS, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, BOTTOM_INSETS))
                        .build();

        ViewCompat.dispatchApplyWindowInsets(dialogRootView, windowInsets);

        View e2eLayout = dialogRootView.findViewById(R.id.edge_to_edge_base_layout);
        Assert.assertEquals(
                "E2E layout has the wrong top padding.", TOP_INSETS, e2eLayout.getPaddingTop());
        Assert.assertEquals(
                "E2E layout has the wrong bottom padding.",
                BOTTOM_INSETS,
                e2eLayout.getPaddingBottom());
    }

    private void assertWrappedWithAutomotiveLayout(View dialogView) {
        Assert.assertFalse(
                "Dialog should not wrapped with e2e layout.",
                dialogView.getParent() instanceof EdgeToEdgeBaseLayout);
        Assert.assertNotNull(
                "Automotive base layout cannot be found.",
                dialogView.getRootView().findViewById(R.id.automotive_base_frame_layout));
    }

    private void assertWrappedWithEdgeToEdgeLayout(View dialogView) {
        Assert.assertTrue(
                "Dialog not wrapped with e2e layout.",
                dialogView.getParent() instanceof EdgeToEdgeBaseLayout);
        Assert.assertNull(
                "Automotive base layout cannot be found.",
                dialogView.getRootView().findViewById(R.id.automotive_base_frame_layout));
    }

    private void launchActivity() {
        assert mActivity == null;
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }
}

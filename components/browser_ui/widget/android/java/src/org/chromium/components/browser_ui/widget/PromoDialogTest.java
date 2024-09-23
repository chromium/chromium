// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.widget.PromoDialog.DialogParams;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.Callable;

/** Tests for the PromoDialog and PromoDialogLayout. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PromoDialogTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    /**
     * Creates a PromoDialog. Doesn't call {@link PromoDialog#show} because there is no Window to
     * attach them to, but it does create them and inflate the layouts.
     */
    private static class PromoDialogWrapper {
        public final CallbackHelper primaryCallback = new CallbackHelper();
        public final CallbackHelper secondaryCallback = new CallbackHelper();
        public final PromoDialog dialog;
        public final PromoDialogLayout dialogLayout;

        private final DialogParams mDialogParams;

        PromoDialogWrapper(final Activity activity, final DialogParams dialogParams)
                throws Exception {
            mDialogParams = dialogParams;
            dialog =
                    ThreadUtils.runOnUiThreadBlocking(
                            new Callable<PromoDialog>() {
                                @Override
                                public PromoDialog call() {
                                    PromoDialog dialog =
                                            new PromoDialog(activity) {
                                                @Override
                                                public DialogParams getDialogParams() {
                                                    return mDialogParams;
                                                }

                                                @Override
                                                public void onDismiss(DialogInterface dialog) {}

                                                @Override
                                                public void onClick(View view) {
                                                    if (view.getId() == R.id.button_primary) {
                                                        primaryCallback.notifyCalled();
                                                    } else if (view.getId()
                                                            == R.id.button_secondary) {
                                                        secondaryCallback.notifyCalled();
                                                    }
                                                }
                                            };
                                    dialog.onCreate(null);
                                    return dialog;
                                }
                            });
            dialogLayout =
                    ThreadUtils.runOnUiThreadBlocking(
                            new Callable<PromoDialogLayout>() {
                                @Override
                                public PromoDialogLayout call() {
                                    PromoDialogLayout promoDialogLayout =
                                            (PromoDialogLayout)
                                                    dialog.getWindow()
                                                            .getDecorView()
                                                            .findViewById(R.id.promo_dialog_layout);
                                    return promoDialogLayout;
                                }
                            });
            // Measure the PromoDialogLayout so that the controls have some size.
            triggerDialogLayoutMeasure(500, 1000);
        }

        /** Trigger a {@link View#measure(int, int)} on the promo dialog layout. */
        public void triggerDialogLayoutMeasure(final int width, final int height) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        int widthMeasureSpec =
                                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
                        int heightMeasureSpec =
                                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
                        dialogLayout.measure(widthMeasureSpec, heightMeasureSpec);
                    });
        }
    }

    private static Activity sActivity;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                });
    }

    @Test
    @SmallTest
    public void testBasic_Visibility() throws Exception {
        // Create a full dialog.
        DialogParams dialogParams = new DialogParams();
        dialogParams.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.subheaderStringResource = R.string.promo_dialog_test_subheader;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        dialogParams.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        dialogParams.footerStringResource = R.string.promo_dialog_test_footer;
        checkDialogControlVisibility(dialogParams);

        // Create a minimal dialog.
        dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_subheader;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        checkDialogControlVisibility(dialogParams);
    }

    /** Confirm that PromoDialogs are constructed with all the elements expected. */
    private void checkDialogControlVisibility(final DialogParams dialogParams) throws Exception {
        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;

        View illustration = promoDialogLayout.findViewById(R.id.illustration);
        View header = promoDialogLayout.findViewById(R.id.header);
        View subheader = promoDialogLayout.findViewById(R.id.subheader);
        View primary = promoDialogLayout.findViewById(R.id.button_primary);
        View secondary = promoDialogLayout.findViewById(R.id.button_secondary);
        View footer = promoDialogLayout.findViewById(R.id.footer);

        // Any controls not specified by the DialogParams won't exist.
        checkControlVisibility(illustration, dialogParams.vectorDrawableResource != 0);
        checkControlVisibility(header, dialogParams.headerStringResource != 0);
        checkControlVisibility(subheader, dialogParams.subheaderStringResource != 0);
        checkControlVisibility(primary, dialogParams.primaryButtonStringResource != 0);
        checkControlVisibility(secondary, dialogParams.secondaryButtonStringResource != 0);
        checkControlVisibility(footer, dialogParams.footerStringResource != 0);
    }

    /** Check if a control should be visible. */
    private void checkControlVisibility(View view, boolean shouldBeVisible) {
        Assert.assertEquals(shouldBeVisible, view != null);
        if (view != null) {
            Assert.assertTrue(view.getMeasuredWidth() > 0);
            Assert.assertTrue(view.getMeasuredHeight() > 0);
        }
    }

    @Test
    @SmallTest
    public void testBasic_CharSequenceSummary() throws Exception {
        final String subheaderCharSequenceTestValue = "Promo dialog CharSequence sub-header";

        // Create basic dialog with subheaderCharSequence.
        // Check that subHeader is visible.
        DialogParams dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.subheaderCharSequence = subheaderCharSequenceTestValue;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;
        TextView subheader = (TextView) promoDialogLayout.findViewById(R.id.subheader);
        checkControlVisibility(subheader, true);

        // Create basic dialog with both subheaderCharSequence and subheaderStringResource.
        // Check that subheaderCharSequence takes precedence.
        dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.subheaderCharSequence = subheaderCharSequenceTestValue;
        dialogParams.subheaderStringResource = R.string.promo_dialog_test_subheader;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        promoDialogLayout = wrapper.dialogLayout;
        subheader = promoDialogLayout.findViewById(R.id.subheader);
        Assert.assertEquals(subheader.getText(), subheaderCharSequenceTestValue);

        // Without setting subHeaderIsLink the sub-header should have the default movement method.
        Assert.assertFalse(subheader.getMovementMethod() instanceof LinkMovementMethod);

        // Create dialog with sub-header as link
        dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.subheaderCharSequence = subheaderCharSequenceTestValue;
        dialogParams.subheaderIsLink = true;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        promoDialogLayout = wrapper.dialogLayout;
        subheader = promoDialogLayout.findViewById(R.id.subheader);
        Assert.assertTrue(subheader.getMovementMethod() instanceof LinkMovementMethod);
    }

    @Test
    @SmallTest
    public void testBasic_Orientation() throws Exception {
        DialogParams dialogParams = new DialogParams();
        dialogParams.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.subheaderStringResource = R.string.promo_dialog_test_subheader;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        dialogParams.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        dialogParams.footerStringResource = R.string.promo_dialog_test_footer;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        final PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;
        LinearLayout flippableLayout =
                (LinearLayout) promoDialogLayout.findViewById(R.id.full_promo_content);

        // Tall screen should keep the illustration above everything else.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int widthMeasureSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY);
                    int heightMeasureSpec = MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY);
                    promoDialogLayout.measure(widthMeasureSpec, heightMeasureSpec);
                });
        Assert.assertEquals(LinearLayout.VERTICAL, flippableLayout.getOrientation());

        // Wide screen should move the image left.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int widthMeasureSpec = MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY);
                    int heightMeasureSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY);
                    promoDialogLayout.measure(widthMeasureSpec, heightMeasureSpec);
                });
        Assert.assertEquals(LinearLayout.HORIZONTAL, flippableLayout.getOrientation());
    }

    @Test
    @SmallTest
    public void testBasic_ButtonClicks() throws Exception {
        DialogParams dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        dialogParams.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        final PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;

        // Nothing should have been clicked yet.
        Assert.assertEquals(0, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(0, wrapper.secondaryCallback.getCallCount());

        // Only the primary button should register a click.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    promoDialogLayout.findViewById(R.id.button_primary).performClick();
                });
        Assert.assertEquals(1, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(0, wrapper.secondaryCallback.getCallCount());

        // Only the secondary button should register a click.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    promoDialogLayout.findViewById(R.id.button_secondary).performClick();
                });
        Assert.assertEquals(1, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(1, wrapper.secondaryCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testBasic_HeaderBehavior_WithIllustration() throws Exception {
        // With an illustration, the header View is part of the scrollable content.
        DialogParams dialogParams = new DialogParams();
        dialogParams.drawableResource = R.drawable.promo_dialog_test_drawable;
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;
        ViewGroup scrollableLayout =
                (ViewGroup) promoDialogLayout.findViewById(R.id.scrollable_promo_content);

        View header = promoDialogLayout.findViewById(R.id.header);
        Assert.assertEquals(scrollableLayout.getChildAt(0), header);
        assertHasStartAndEndPadding(header, false);
    }

    @Test
    @SmallTest
    public void testBasic_HeaderBehavior_WithVectorIllustration() throws Exception {
        // With a vector illustration, the header View is part of the scrollable content.
        DialogParams dialogParams = new DialogParams();
        dialogParams.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;
        ViewGroup scrollableLayout =
                (ViewGroup) promoDialogLayout.findViewById(R.id.scrollable_promo_content);

        View header = promoDialogLayout.findViewById(R.id.header);
        Assert.assertEquals(scrollableLayout.getChildAt(0), header);
        assertHasStartAndEndPadding(header, false);
    }

    @Test
    @SmallTest
    public void testBasic_HeaderBehavior_NoIllustration() throws Exception {
        // Without an illustration, the header View becomes locked to the top of the layout if
        // there is enough height.
        DialogParams dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.promo_dialog_test_header;
        dialogParams.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(sActivity, dialogParams);
        PromoDialogLayout promoDialogLayout = wrapper.dialogLayout;

        // Add a dummy control view to ensure the scrolling container has some content.
        View view = new View(InstrumentationRegistry.getTargetContext());
        view.setMinimumHeight(2000);
        promoDialogLayout.addControl(view);

        View header = promoDialogLayout.findViewById(R.id.header);
        ViewGroup scrollableLayout =
                (ViewGroup) promoDialogLayout.findViewById(R.id.scrollable_promo_content);

        wrapper.triggerDialogLayoutMeasure(400, 2000);
        Assert.assertEquals(promoDialogLayout.getChildAt(0), header);
        assertHasStartAndEndPadding(header, true);

        // Decrease the size and see the header is moved into the scrollable content.
        wrapper.triggerDialogLayoutMeasure(400, 100);
        Assert.assertEquals(scrollableLayout.getChildAt(0), header);
        assertHasStartAndEndPadding(header, false);

        // Increase again and ensure the header is moved back to the top of the layout.
        wrapper.triggerDialogLayoutMeasure(400, 2000);
        Assert.assertEquals(promoDialogLayout.getChildAt(0), header);
        assertHasStartAndEndPadding(header, true);
    }

    private static void assertHasStartAndEndPadding(View view, boolean shouldHavePadding) {
        if (shouldHavePadding) {
            Assert.assertNotEquals(0, ViewCompat.getPaddingStart(view));
            Assert.assertNotEquals(0, ViewCompat.getPaddingEnd(view));
        } else {
            Assert.assertEquals(0, ViewCompat.getPaddingStart(view));
            Assert.assertEquals(0, ViewCompat.getPaddingEnd(view));
        }
    }
}

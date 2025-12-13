// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static com.google.common.truth.Truth.assertThat;

import android.app.Dialog;
import android.os.Bundle;
import android.widget.FrameLayout;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.ui.test.util.BlankUiTestActivity;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FullscreenAlertDialogTest {

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static BlankUiTestActivity sActivity;

    @BeforeClass
    public static void beforeClass() {
        sActivity = activityTestRule.launchActivity(null);
    }

    @Test
    @SmallTest
    public void testCreateAndShowFromFragment() {
        var fragment = new TestDialogFragment();
        fragment.show(sActivity.getSupportFragmentManager(), "");

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(fragment.mDialog.isShowing(), Matchers.is(true)));
    }

    @Test
    @SmallTest
    public void testCreateAndShowFromConstructor() {
        PayloadCallbackHelper<FullscreenAlertDialog> callbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThread(
                () -> {
                    var dialog =
                            new FullscreenAlertDialog(sActivity, /* shouldPadForContent= */ true);
                    callbackHelper.notifyCalled(dialog);
                    dialog.show();
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                callbackHelper.getOnlyPayloadBlocking().isShowing(),
                                Matchers.is(true)));
    }

    public static class TestDialogFragment extends DialogFragment {
        AlertDialog mDialog;

        @Override
        public @NonNull Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
            assertThat(getActivity()).isNotNull();
            FrameLayout dialogContent = new FrameLayout(getActivity());
            mDialog =
                    new FullscreenAlertDialog.Builder(
                                    getActivity(), /* shouldPadForContent= */ true)
                            .setView(dialogContent)
                            .create();

            return mDialog;
        }
    }
}

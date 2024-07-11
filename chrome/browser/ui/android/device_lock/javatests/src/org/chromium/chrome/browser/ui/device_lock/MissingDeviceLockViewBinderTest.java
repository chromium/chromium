// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CHECKBOX_TOGGLED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.REMOVE_ALL_LOCAL_DATA_CHECKED;

import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link MissingDeviceLockViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MissingDeviceLockViewBinderTest extends BlankUiTestActivityTestCase {
    private AtomicBoolean mCreateDeviceLockButtonClicked = new AtomicBoolean();
    private AtomicBoolean mContinueClicked = new AtomicBoolean();
    private AtomicBoolean mCheckboxToggled = new AtomicBoolean();

    private MissingDeviceLockView mView;
    private PropertyModel mViewModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view);

                    mView = MissingDeviceLockView.create(getActivity().getLayoutInflater());
                    view.addView(mView);

                    mViewModel =
                            new PropertyModel.Builder(MissingDeviceLockProperties.ALL_KEYS)
                                    .with(REMOVE_ALL_LOCAL_DATA_CHECKED, true)
                                    .with(
                                            MissingDeviceLockProperties
                                                    .ON_CREATE_DEVICE_LOCK_CLICKED,
                                            v -> mCreateDeviceLockButtonClicked.set(true))
                                    .with(ON_CONTINUE_CLICKED, v -> mContinueClicked.set(true))
                                    .with(
                                            ON_CHECKBOX_TOGGLED,
                                            (v, isChecked) -> mCheckboxToggled.set(isChecked))
                                    .build();

                    mModelChangeProcessor =
                            PropertyModelChangeProcessor.create(
                                    mViewModel, mView, MissingDeviceLockViewBinder::bind);
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mModelChangeProcessor::destroy);
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testMissingDeviceLockView_createDeviceLockClicked_triggersOnClick() {
        mCreateDeviceLockButtonClicked.set(false);

        mView.getCreateDeviceLockButton().performClick();
        assertTrue(
                "A click on the create device lock button should trigger the handler.",
                mCreateDeviceLockButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testMissingDeviceLockView_continueButtonClicked_triggersOnClick() {
        mContinueClicked.set(false);

        mView.getContinueButton().performClick();
        assertTrue(
                "A click on the continue button should trigger the continue handler.",
                mContinueClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testMissingDeviceLockView_checkboxToggled() {
        mView.getCheckbox().setChecked(true);
        mView.getCheckbox().performClick();
        assertFalse("The checkbox should be toggled off.", mCheckboxToggled.get());
        mView.getCheckbox().performClick();
        assertTrue("The checkbox should be toggled on.", mCheckboxToggled.get());
    }
}

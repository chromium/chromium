// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabModalPresenter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
public class TabModalPresenterUnitTest {
    /** Minimal concrete presenter; the container stands in for the tab modal container. */
    private static class TestTabModalPresenter extends TabModalPresenter {
        private final FrameLayout mContainer;

        TestTabModalPresenter(Context context, FrameLayout container) {
            super(context);
            mContainer = container;
        }

        @Override
        protected ViewGroup createDialogContainer() {
            return mContainer;
        }

        @Override
        protected void showDialogContainer() {}

        @Override
        protected void setBrowserControlsAccess(boolean restricted) {}
    }

    private Activity mActivity;
    private FrameLayout mContainer;
    private TestTabModalPresenter mPresenter;

    @Before
    public void setUp() {
        mActivity = buildActivity(TestActivity.class).setup().get();
        mContainer = new FrameLayout(mActivity);
        mPresenter = new TestTabModalPresenter(mActivity, mContainer);
    }

    private MarginLayoutParams showDialogAndGetLayoutParams() {
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.TITLE, "Title")
                        .build();
        mPresenter.addDialogView(model, /* onDialogCreatedCallback= */ null, null);
        mPresenter.runEnterAnimation();
        return (MarginLayoutParams) mContainer.getChildAt(0).getLayoutParams();
    }

    @Test
    @EnableFeatures(ModalDialogFeatureList.DIALOGS_ON_LARGE_FORM_FACTORS)
    public void addDialogView_LargeFormFactorUi_SetsLayoutMargins() {
        int expectedHorizontalMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modal_dialog_view_horizontal_margin_lff);
        int expectedVerticalMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modal_dialog_view_vertical_margin_lff);

        MarginLayoutParams params = showDialogAndGetLayoutParams();

        assertEquals("Wrong left margin.", expectedHorizontalMargin, params.leftMargin);
        assertEquals("Wrong right margin.", expectedHorizontalMargin, params.rightMargin);
        assertEquals("Wrong top margin.", expectedVerticalMargin, params.topMargin);
        assertEquals("Wrong bottom margin.", expectedVerticalMargin, params.bottomMargin);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures(ModalDialogFeatureList.DIALOGS_ON_LARGE_FORM_FACTORS)
    public void addDialogView_Phone_NoLayoutMargins() {
        // The flag is on; the form factor alone keeps the margins off.
        MarginLayoutParams params = showDialogAndGetLayoutParams();

        assertEquals("Wrong left margin.", 0, params.leftMargin);
        assertEquals("Wrong right margin.", 0, params.rightMargin);
        assertEquals("Wrong top margin.", 0, params.topMargin);
        assertEquals("Wrong bottom margin.", 0, params.bottomMargin);
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewAndroidDelegate;
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
    public void addDialogView_LargeFormFactorUi_SetsHorizontalLayoutMarginsOnly() {
        int expectedHorizontalMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modal_dialog_view_horizontal_margin_lff);

        MarginLayoutParams params = showDialogAndGetLayoutParams();

        assertEquals("Wrong left margin.", expectedHorizontalMargin, params.leftMargin);
        assertEquals("Wrong right margin.", expectedHorizontalMargin, params.rightMargin);
        assertEquals("Wrong top margin.", 0, params.topMargin);
        assertEquals("Wrong bottom margin.", 0, params.bottomMargin);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void addDialogView_Phone_NoLayoutMargins() {
        // The flag is on; the form factor alone keeps the margins off.
        MarginLayoutParams params = showDialogAndGetLayoutParams();

        assertEquals("Wrong left margin.", 0, params.leftMargin);
        assertEquals("Wrong right margin.", 0, params.rightMargin);
        assertEquals("Wrong top margin.", 0, params.topMargin);
        assertEquals("Wrong bottom margin.", 0, params.bottomMargin);
    }

    /**
     * Sets up the WebContents' container view, inside a focusable parent that takes focus when the
     * container view loses it, like CompositorViewHolder in Chrome.
     *
     * @return The container view, not yet focused.
     */
    private View setUpContainerView(WebContents webContents) {
        FrameLayout containerView = new FrameLayout(mActivity);
        containerView.setFocusable(true);
        FrameLayout compositorViewHolder = new FrameLayout(mActivity);
        compositorViewHolder.setFocusable(true);
        compositorViewHolder.addView(containerView);
        mActivity.setContentView(compositorViewHolder);
        when(webContents.getViewAndroidDelegate())
                .thenReturn(ViewAndroidDelegate.createBasicDelegate(containerView));
        when(webContents.getOrSetUserData(eq(SelectionPopupControllerImpl.class), any()))
                .thenReturn(mock(SelectionPopupControllerImpl.class));
        return containerView;
    }

    @Test
    public void saveOrRestoreTextSelection_RestoreFocus_RestoresContainerViewFocus() {
        WebContents webContents = mock(WebContents.class);
        View containerView = setUpContainerView(webContents);
        containerView.requestFocus();

        mPresenter.saveOrRestoreTextSelection(
                webContents, /* save= */ true, /* restoreFocus= */ false);
        assertFalse("Container view should lose focus.", containerView.hasFocus());

        mPresenter.saveOrRestoreTextSelection(
                webContents, /* save= */ false, /* restoreFocus= */ true);
        assertTrue("Container view should get focus back.", containerView.hasFocus());
    }

    @Test
    public void saveOrRestoreTextSelection_NoRestoreFocus_DoesNotRestoreContainerViewFocus() {
        WebContents webContents = mock(WebContents.class);
        View containerView = setUpContainerView(webContents);
        containerView.requestFocus();

        mPresenter.saveOrRestoreTextSelection(
                webContents, /* save= */ true, /* restoreFocus= */ false);
        mPresenter.saveOrRestoreTextSelection(
                webContents, /* save= */ false, /* restoreFocus= */ false);
        assertFalse("Container view should not get focus.", containerView.hasFocus());
    }

    @Test
    public void saveOrRestoreTextSelection_RestoreWithoutSave_DoesNotFocusContainerView() {
        WebContents webContents = mock(WebContents.class);
        View containerView = setUpContainerView(webContents);
        assertFalse("Container view should start unfocused.", containerView.hasFocus());

        mPresenter.saveOrRestoreTextSelection(
                webContents, /* save= */ false, /* restoreFocus= */ true);
        assertFalse("Container view should not get focus.", containerView.hasFocus());
    }
}

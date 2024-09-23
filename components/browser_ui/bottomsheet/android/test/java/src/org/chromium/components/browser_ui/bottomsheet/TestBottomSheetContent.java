// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CallbackHelper;

/** A simple sheet content to test with. This only displays two empty white views. */
public class TestBottomSheetContent implements BottomSheetContent {
    /** The height of the toolbar for this test content. */
    public static final int TOOLBAR_HEIGHT = 100;

    /** {@link CallbackHelper} to ensure the destroy method is called. */
    public final CallbackHelper destroyCallbackHelper = new CallbackHelper();

    /** Empty view that represents the toolbar. */
    private View mToolbarView;

    /** Empty view that represents the content. */
    private View mContentView;

    /** This content's priority. */
    private @ContentPriority int mPriority;

    /** Whether this content is browser specific. */
    private boolean mHasCustomLifecycle;

    /** Whether this content has a custom scrim lifecycle. */
    private boolean mHasCustomScrimLifecycle;

    /** The peek height of this content. */
    private int mPeekHeight;

    /** The half height of this content. */
    private float mHalfHeight;

    /** The full height of this content. */
    private float mFullHeight;

    /** If set to true, the half state will be skipped when scrolling down the FULL sheet. */
    private boolean mSkipHalfStateScrollingDown;

    /** Whether this content intercepts back button presses. */
    private boolean mHandleBackPress;

    private ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier;

    /**
     * Whether this content can be immediately replaced by higher-priority content even while the
     * sheet is open.
     */
    private boolean mCanSuppressInAnyState;

    /**
     * @param context A context to inflate views with.
     * @param priority The content's priority.
     * @param hasCustomLifecycle Whether the content is browser specific.
     * @param contentView The view filling the sheet.
     */
    public TestBottomSheetContent(
            Context context,
            @ContentPriority int priority,
            boolean hasCustomLifecycle,
            View contentView) {
        mPeekHeight = BottomSheetContent.HeightMode.DEFAULT;
        mHalfHeight = BottomSheetContent.HeightMode.DEFAULT;
        mFullHeight = BottomSheetContent.HeightMode.DEFAULT;
        mPriority = priority;
        mHasCustomLifecycle = hasCustomLifecycle;
        mCanSuppressInAnyState = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbarView = new View(context);
                    ViewGroup.LayoutParams params =
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT, TOOLBAR_HEIGHT);
                    mToolbarView.setLayoutParams(params);
                    mToolbarView.setBackground(new ColorDrawable(Color.WHITE));

                    if (contentView == null) {
                        mContentView = new View(context);
                        params =
                                new ViewGroup.LayoutParams(
                                        ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.MATCH_PARENT);
                        mContentView.setLayoutParams(params);
                    } else {
                        mContentView = contentView;
                    }
                    mToolbarView.setBackground(new ColorDrawable(Color.WHITE));
                });
    }

    /**
     * @param context A context to inflate views with.
     * @param priority The content's priority.
     * @param hasCustomLifecycle Whether the content is browser specific.
     */
    public TestBottomSheetContent(
            Context context, @ContentPriority int priority, boolean hasCustomLifecycle) {
        this(context, priority, hasCustomLifecycle, null);
    }

    /** @param context A context to inflate views with. */
    public TestBottomSheetContent(Context context) {
        this(/*TestBottomSheetContent(*/ context, ContentPriority.LOW, false);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        destroyCallbackHelper.notifyCalled();
    }

    @Override
    public int getPriority() {
        return mPriority;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    public void setSkipHalfStateScrollingDown(boolean skiphalfStateScrollingDown) {
        mSkipHalfStateScrollingDown = skiphalfStateScrollingDown;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return mSkipHalfStateScrollingDown;
    }

    public void setPeekHeight(int height) {
        mPeekHeight = height;
    }

    @Override
    public int getPeekHeight() {
        return mPeekHeight;
    }

    public void setHalfHeightRatio(float ratio) {
        mHalfHeight = ratio;
    }

    @Override
    public float getHalfHeightRatio() {
        return mHalfHeight;
    }

    public void setFullHeightRatio(float ratio) {
        mFullHeight = ratio;
    }

    @Override
    public float getFullHeightRatio() {
        return mFullHeight;
    }

    public void setHasCustomScrimLifecycle(boolean hasCustomScrimLifecycle) {
        mHasCustomScrimLifecycle = hasCustomScrimLifecycle;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return mHasCustomScrimLifecycle;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return mHasCustomLifecycle;
    }

    @Override
    public boolean handleBackPress() {
        return mHandleBackPress;
    }

    public void setHandleBackPress(boolean handleBackPress) {
        getBackPressStateChangedSupplier().set(handleBackPress);
        mHandleBackPress = handleBackPress;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        if (mBackPressStateChangedSupplier == null) {
            mBackPressStateChangedSupplier = new ObservableSupplierImpl<>();
            mBackPressStateChangedSupplier.set(false);
        }
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        setHandleBackPress(false);
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return android.R.string.copy;
    }

    @Override
    public boolean canSuppressInAnyState() {
        return mCanSuppressInAnyState;
    }

    public void setCanSuppressInAnyState(boolean value) {
        mCanSuppressInAnyState = value;
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Presentation view layer for the BottomSheet component. */
@NullMarked
public class BottomSheetView extends FrameLayout {
    private static final GlowSpec DEFAULT_GLOW_SPEC = new GlowSpec(0, GlowSpec.ShadowSize.DEFAULT);

    /** The visual presentation layout modes supported by the bottom sheet. */
    @IntDef({
        SheetLayoutMode.STANDARD,
        SheetLayoutMode.DESKTOP_POPUP,
        SheetLayoutMode.DESKTOP_FALLBACK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetLayoutMode {
        /** Standard mobile bottom sheet presentation. */
        int STANDARD = 0;

        /** Desktop popup presentation with a surrounding shadow frame and explicit close button. */
        int DESKTOP_POPUP = 1;

        /**
         * Desktop fallback presentation with standard mobile sheet bounds and fallback bottom
         * shadow.
         */
        int DESKTOP_FALLBACK = 2;
    }

    /**
     * A view used to render a shadow behind the sheet and extends outside the bounds of its parent
     * view.
     */
    public static class ShadowLayerView extends View {
        /** The length of the shadow in any direction. */
        private int mShadowLength;

        /**
         * Constructor to inflate from XML.
         *
         * @param context The Context the view is running in.
         * @param atts The attributes of the XML tag inflating the view.
         */
        public ShadowLayerView(Context context, @Nullable AttributeSet atts) {
            super(context, atts);
            Resources resources = context.getResources();
            setShadowLength(resources.getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length));
        }

        /**
         * Sets the length of the shadow.
         *
         * @param length The length of the shadow in pixels.
         */
        public void setShadowLength(int length) {
            mShadowLength = length;
            setTranslationX((LocalizationUtils.isLayoutRtl() ? 1 : -1) * mShadowLength);
            setTranslationY(-mShadowLength);
            ViewUtils.requestLayout(this, "BottomSheetView.ShadowLayerView.setShadowLength");
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(widthMeasureSpec) + 2 * mShadowLength,
                            MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(heightMeasureSpec) + mShadowLength,
                            MeasureSpec.EXACTLY));
        }
    }

    /** An out-array for use with getLocationOnScreen to prevent constant allocations. */
    private final int[] mCachedLocation = new int[2];

    /** The shadow length in pixels for standard sheets. */
    private final @Px int mShadowLength;

    /** The shadow length in pixels for large sheets. */
    private final @Px int mShadowLengthLarge;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /** The FrameLayout used to hold the bottom sheet toolbar. */
    private TouchRestrictingFrameLayout mToolbarHolder;

    /** The view that contains the sheet background color. */
    private View mSheetBackground;

    /** The view that contains the sheet background glow color. */
    private View mShadowLayer;

    /**
     * The view that is used to cover the area below the bottom sheet contents that is normally
     * obscured by the keyboard.
     */
    private View mKeyboardCurtain;

    /**
     * The optional 'X' close button. This is injected into large form factor layouts when the sheet
     * is non-modal (meaning it lacks a background scrim that would otherwise allow the user to
     * easily tap-to-dismiss).
     */
    private @Nullable View mCloseButton;

    /**
     * An alternative shadow layer used exclusively on large form factor devices when the current
     * sheet content opts out of the new bottom sheet UI. This provides the standard mobile
     * bottom-edge bleeder shadow instead of the full perimeter rectangle shadow.
     */
    private @Nullable View mFallbackShadowLayer;

    /** The drag handlebar view shown at the top of the sheet when requested by content. */
    private ImageView mHandlebar;

    /** The active visual layout mode of the sheet. */
    private @SheetLayoutMode int mLayoutMode = SheetLayoutMode.STANDARD;

    /** The visible cosmetic height of the sheet background and shadow layer. */
    private @Px int mVisibleBackgroundHeight;

    /** The active glow specification for the sheet. */
    private @Nullable GlowSpec mGlowSpec;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Context the view is running in.
     * @param atts The attributes of the XML tag inflating the view.
     */
    public BottomSheetView(Context context, @Nullable AttributeSet atts) {
        super(context, atts);
        Resources resources = context.getResources();
        mShadowLength = resources.getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
        mShadowLengthLarge =
                resources.getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length_large);
    }

    @Initializer
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBottomSheetContentContainer = findViewById(R.id.bottom_sheet_content);
        mToolbarHolder = findViewById(R.id.bottom_sheet_toolbar_container);
        mSheetBackground = findViewById(R.id.background);
        mShadowLayer = findViewById(R.id.shadow_layer);
        mKeyboardCurtain = findViewById(R.id.keyboard_curtain);
        mCloseButton = findViewById(R.id.bottom_sheet_close_button);
        mFallbackShadowLayer = findViewById(R.id.desktop_fallback_shadow);
        mHandlebar = findViewById(R.id.handlebar);
    }

    /** Returns the current visual presentation layout mode. */
    public @SheetLayoutMode int getSheetLayoutMode() {
        return mLayoutMode;
    }

    /**
     * Sets whether touch events are enabled on the content and toolbar containers.
     *
     * @param isTouchEnabled Whether touch is enabled.
     */
    public void setContainerTouchEnabled(boolean isTouchEnabled) {
        if (mBottomSheetContentContainer != null) {
            mBottomSheetContentContainer.setIsTouchEnabled(isTouchEnabled);
        }
        if (mToolbarHolder != null) {
            mToolbarHolder.setIsTouchEnabled(isTouchEnabled);
        }
    }

    /**
     * Sets the width of the sheet container.
     *
     * @param width The target width in pixels.
     */
    public void setSheetWidth(@Px int width) {
        ViewGroup.LayoutParams params = getLayoutParams();
        if (params != null && params.width != width) {
            params.width = width;
            setLayoutParams(params);
        }
    }

    /**
     * Sets the background color of the sheet background and keyboard curtain.
     *
     * @param color The color to tint the sheet background and keyboard curtain.
     */
    public void setSheetBackgroundColor(@ColorInt int color) {
        ColorStateList tintList = ColorStateList.valueOf(color);
        if (mSheetBackground != null) {
            mSheetBackground.setBackgroundTintList(tintList);
        }
        if (mKeyboardCurtain != null) {
            mKeyboardCurtain.setBackgroundTintList(tintList);
        }
    }

    /**
     * Sets the accessibility pane title for the sheet view.
     *
     * @param title The pane title.
     */
    public void setSheetAccessibilityPaneTitle(@Nullable CharSequence title) {
        ViewCompat.setAccessibilityPaneTitle(this, title);
    }

    private void setFallbackShadowVisible(boolean visible) {
        if (mFallbackShadowLayer != null) {
            mFallbackShadowLayer.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Sets whether the close button is visible.
     *
     * @param visible Whether the close button should be visible.
     */
    public void setCloseButtonVisible(boolean visible) {
        if (mCloseButton != null) {
            mCloseButton.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Sets the click listener for the close button.
     *
     * @param listener The click listener, or null.
     */
    public void setCloseButtonClickListener(@Nullable OnClickListener listener) {
        if (mCloseButton != null) {
            mCloseButton.setOnClickListener(listener);
        }
    }

    /**
     * Updates margins on the shadow layer depending on popup mode.
     *
     * @param isPopup Whether the sheet is in popup layout mode.
     */
    private void updateShadowLayerMargins(boolean isPopup) {
        if (mShadowLayer == null) return;
        if (!(mShadowLayer.getLayoutParams() instanceof MarginLayoutParams)) return;
        MarginLayoutParams lp = (MarginLayoutParams) mShadowLayer.getLayoutParams();
        if (isPopup) {
            lp.setMargins(
                    -mShadowLayer.getPaddingLeft(),
                    -mShadowLayer.getPaddingTop(),
                    -mShadowLayer.getPaddingRight(),
                    -mShadowLayer.getPaddingBottom());
        } else {
            lp.setMargins(0, 0, 0, 0);
        }
        mShadowLayer.setLayoutParams(lp);
    }

    /**
     * Sets the visual layout mode of the sheet.
     *
     * @param mode The layout mode to apply.
     */
    public void setSheetLayoutMode(@SheetLayoutMode int mode) {
        if (mLayoutMode == mode) return;
        mLayoutMode = mode;
        switch (mode) {
            case SheetLayoutMode.DESKTOP_POPUP -> {
                if (mSheetBackground != null) {
                    mSheetBackground.setBackgroundResource(
                            R.drawable.bottom_sheet_desktop_background);
                    mSheetBackground.setClipToOutline(true);
                }
                setFallbackShadowVisible(false);
                if (mShadowLayer != null) {
                    mShadowLayer.setBackgroundResource(R.drawable.popup_bg_shadow_16dp);
                    updateShadowLayerMargins(true);
                }
            }
            case SheetLayoutMode.DESKTOP_FALLBACK -> {
                setCloseButtonVisible(false);
                if (mSheetBackground != null) {
                    mSheetBackground.setBackgroundResource(R.drawable.bottom_sheet_background);
                    mSheetBackground.setClipToOutline(false);
                }
                setFallbackShadowVisible(true);
                if (mShadowLayer != null) {
                    mShadowLayer.setBackgroundResource(0);
                    mShadowLayer.setPadding(0, 0, 0, 0);
                    updateShadowLayerMargins(false);
                }
                if (mGlowSpec != null) {
                    setGlowSpec(mGlowSpec);
                }
            }
            default -> {
                setCloseButtonVisible(false);
                if (mSheetBackground != null) {
                    mSheetBackground.setBackgroundResource(R.drawable.bottom_sheet_background);
                    mSheetBackground.setClipToOutline(false);
                }
                setFallbackShadowVisible(false);
                if (mShadowLayer != null) {
                    updateShadowLayerMargins(false);
                    if (mFallbackShadowLayer != null) {
                        mShadowLayer.setBackgroundResource(0);
                    }
                }
            }
        }
    }

    /**
     * Sets the height of the keyboard curtain view.
     *
     * @param height The height in pixels.
     */
    public void setKeyboardCurtainHeight(@Px int height) {
        if (mKeyboardCurtain == null) return;
        mKeyboardCurtain.setTranslationY(height);
        MarginLayoutParams params = (MarginLayoutParams) mKeyboardCurtain.getLayoutParams();
        if (params != null && params.height != height) {
            params.height = height;
            mKeyboardCurtain.setLayoutParams(params);
        }
    }

    /**
     * Sets the glow specification and updates shadow parameters.
     *
     * @param spec The glow specification to apply.
     */
    public void setGlowSpec(GlowSpec spec) {
        mGlowSpec = spec;
        if (mLayoutMode == SheetLayoutMode.DESKTOP_POPUP) return;
        View shadowLayer =
                (mFallbackShadowLayer != null
                                && mFallbackShadowLayer.getVisibility() == View.VISIBLE)
                        ? mFallbackShadowLayer
                        : mShadowLayer;
        if (shadowLayer == null) return;

        if (spec.equals(DEFAULT_GLOW_SPEC)) {
            shadowLayer.setBackgroundTintList(null);
        } else {
            shadowLayer.setBackgroundTintList(ColorStateList.valueOf(spec.color));
        }
        shadowLayer.setBackgroundResource(R.drawable.top_round_shadow);
        int shadowSize = spec.size == GlowSpec.ShadowSize.LONG ? mShadowLengthLarge : mShadowLength;
        if (shadowLayer instanceof ShadowLayerView shadowLayerView) {
            shadowLayerView.setShadowLength(shadowSize);
        }
    }

    /** Returns the current glow specification. */
    public @Nullable GlowSpec getGlowSpec() {
        return mGlowSpec;
    }

    void setSheetBackgroundForTesting(View sheetBackground) {
        mSheetBackground = sheetBackground;
    }

    void setShadowLayerForTesting(View shadowLayer) {
        mShadowLayer = shadowLayer;
    }

    void setToolbarHolderForTesting(TouchRestrictingFrameLayout toolbarHolder) {
        mToolbarHolder = toolbarHolder;
    }

    void setBottomSheetContentContainerForTesting(
            TouchRestrictingFrameLayout bottomSheetContentContainer) {
        mBottomSheetContentContainer = bottomSheetContentContainer;
    }

    void setFallbackShadowLayerForTesting(View fallbackShadowLayer) {
        mFallbackShadowLayer = fallbackShadowLayer;
    }

    void setCloseButtonForTesting(View closeButton) {
        mCloseButton = closeButton;
    }

    void setHandlebarForTesting(ImageView handlebar) {
        mHandlebar = handlebar;
    }

    ImageView getHandlebarForTesting() {
        return mHandlebar;
    }

    /**
     * Sets the content view inside the bottom sheet content container.
     *
     * @param view The content view to display, or null to clear.
     */
    public void setContentView(@Nullable View view) {
        if (mBottomSheetContentContainer == null) return;
        if (mBottomSheetContentContainer.getChildCount() == 1
                && mBottomSheetContentContainer.getChildAt(0) == view) {
            return;
        }
        mBottomSheetContentContainer.removeAllViews();
        if (view != null) {
            if (view.getParent() instanceof ViewGroup parent) {
                parent.removeView(view);
            }
            mBottomSheetContentContainer.addView(view);
        }
    }

    /**
     * Sets the toolbar view inside the toolbar holder container.
     *
     * @param view The toolbar view to display, or null to clear.
     */
    public void setToolbarView(@Nullable View view) {
        if (mToolbarHolder == null) return;
        if (mToolbarHolder.getChildCount() == 1 && mToolbarHolder.getChildAt(0) == view) {
            return;
        }
        mToolbarHolder.removeAllViews();
        if (view != null) {
            if (view.getParent() instanceof ViewGroup parent) {
                parent.removeView(view);
            }
            mToolbarHolder.addView(view);
        }
    }

    /**
     * Sets the height of the content container.
     *
     * @param height The target height in pixels.
     */
    public void setContainerHeight(@Px int height) {
        if (mBottomSheetContentContainer == null) return;
        ViewGroup.LayoutParams params = mBottomSheetContentContainer.getLayoutParams();
        if (params != null && params.height != height) {
            params.height = height;
            mBottomSheetContentContainer.setLayoutParams(params);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mVisibleBackgroundHeight > 0) {
            setVisibleBackgroundHeight(mVisibleBackgroundHeight);
        }
    }

    /**
     * Sets the translation Y of the sheet view.
     *
     * @param translationY The translation Y coordinate.
     */
    public void setSheetTranslationY(float translationY) {
        setTranslationY(translationY);
    }

    /**
     * Sets the translation X of the sheet view.
     *
     * @param translationX The translation X coordinate.
     */
    public void setSheetTranslationX(float translationX) {
        setTranslationX(translationX);
    }

    /**
     * Sets whether the drag handlebar is visible.
     *
     * @param visible True if the handlebar should be visible, false otherwise.
     */
    public void setHandlebarVisible(boolean visible) {
        if (mHandlebar != null) {
            mHandlebar.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Sets the top margin of the content container and toolbar holder.
     *
     * @param topMargin The top margin in pixels.
     */
    public void setContentTopMargin(@Px int topMargin) {
        if (mBottomSheetContentContainer != null) {
            MarginLayoutParams params =
                    (MarginLayoutParams) mBottomSheetContentContainer.getLayoutParams();
            if (params != null && params.topMargin != topMargin) {
                params.topMargin = topMargin;
                mBottomSheetContentContainer.setLayoutParams(params);
            }
        }
        if (mToolbarHolder != null) {
            MarginLayoutParams params = (MarginLayoutParams) mToolbarHolder.getLayoutParams();
            if (params != null && params.topMargin != topMargin) {
                params.topMargin = topMargin;
                mToolbarHolder.setLayoutParams(params);
            }
        }
    }

    /**
     * Sets the visible cosmetic height of the sheet background and shadow layer.
     *
     * @param visibleHeight The visible background height in pixels.
     */
    public void setVisibleBackgroundHeight(@Px int visibleHeight) {
        mVisibleBackgroundHeight = visibleHeight;
        if (mSheetBackground == null || mShadowLayer == null) {
            return;
        }

        // Clip the solid background strictly to the visual height.
        mSheetBackground.setBottom(mSheetBackground.getTop() + visibleHeight);

        // Wrap the shadow layer around the new background height, explicitly appending
        // the shadow's native padding to allow the 9-patch border to paint correctly.
        int shadowTopPadding = mShadowLayer.getPaddingTop();
        int shadowBottomPadding = mShadowLayer.getPaddingBottom();
        mShadowLayer.setBottom(
                mShadowLayer.getTop() + visibleHeight + shadowTopPadding + shadowBottomPadding);
        mShadowLayer.invalidate();
    }

    /**
     * Sets whether the sheet view is focusable and manages its focus.
     *
     * @param focusable True if the sheet should be focusable, false otherwise.
     */
    public void setSheetFocusable(boolean focusable) {
        setFocusable(focusable);
        setFocusableInTouchMode(focusable);
        if (focusable) {
            if (getFocusedChild() == null) {
                requestFocus();
            }
        } else {
            clearFocus();
        }
    }

    /**
     * Sets the click listener for the drag handlebar.
     *
     * @param listener The click listener.
     */
    public void setHandlebarClickListener(OnClickListener listener) {
        if (mHandlebar != null) {
            mHandlebar.setOnClickListener(listener);
        }
    }

    /**
     * Sets the pointer icon for the drag handlebar.
     *
     * @param icon The pointer icon.
     */
    public void setHandlebarPointerIcon(@Nullable PointerIcon icon) {
        if (mHandlebar != null) {
            mHandlebar.setPointerIcon(icon);
        }
    }

    /**
     * Measures and returns the measured height of the drag handlebar.
     *
     * @param maxSheetWidth The maximum sheet width for measurement.
     * @param maxSheetHeight The maximum sheet height for measurement.
     * @return The measured height of the handlebar in pixels.
     */
    public @Px int getHandlebarMeasuredHeight(int maxSheetWidth, int maxSheetHeight) {
        if (mHandlebar == null) return 0;
        if (mHandlebar.getMeasuredHeight() == 0) {
            mHandlebar.measure(
                    MeasureSpec.makeMeasureSpec(maxSheetWidth, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(maxSheetHeight, MeasureSpec.AT_MOST));
        }
        return mHandlebar.getMeasuredHeight();
    }

    /**
     * Adds a layout change listener to the toolbar holder.
     *
     * @param listener The layout change listener.
     */
    public void addToolbarLayoutChangeListener(OnLayoutChangeListener listener) {
        if (mToolbarHolder != null) {
            mToolbarHolder.addOnLayoutChangeListener(listener);
        }
    }

    /**
     * Sets the background color of the toolbar holder.
     *
     * @param color The background color int.
     */
    public void setToolbarBackgroundColor(@ColorInt int color) {
        if (mToolbarHolder != null) {
            mToolbarHolder.setBackgroundColor(color);
        }
    }

    /**
     * Returns whether the given touch event falls within the toolbar.
     *
     * @param event The motion event.
     * @return True if the touch event is within the toolbar, false otherwise.
     */
    public boolean isEventInToolbar(MotionEvent event) {
        if (mToolbarHolder == null) return false;
        mToolbarHolder.getLocationOnScreen(mCachedLocation);

        // This check only tests for collision for the Y component since the sheet is the full width
        // of the screen. We only care if the touch event is above the bottom of the toolbar since
        // we won't receive an event if the touch is outside the sheet.
        return mCachedLocation[1] + mToolbarHolder.getHeight() > event.getRawY();
    }

    /**
     * Checks if the content container layout params height differs from the specified height.
     *
     * @param height The target height in pixels.
     * @return True if the current height differs from the specified height, false otherwise.
     */
    public boolean isContentContainerHeightDifferent(int height) {
        if (mBottomSheetContentContainer == null) return false;
        var params = mBottomSheetContentContainer.getLayoutParams();
        return params != null && params.height != height;
    }

    /**
     * Sets bottom padding on the content container.
     *
     * @param paddingBottom The bottom padding in pixels.
     */
    public void setContentContainerPaddingBottom(@Px int paddingBottom) {
        if (mBottomSheetContentContainer == null) return;
        if (mBottomSheetContentContainer.getPaddingBottom() != paddingBottom) {
            mBottomSheetContentContainer.setPadding(
                    mBottomSheetContentContainer.getPaddingLeft(),
                    mBottomSheetContentContainer.getPaddingTop(),
                    mBottomSheetContentContainer.getPaddingRight(),
                    paddingBottom);
        }
    }

    /**
     * Updates background layout params height.
     *
     * @param height The target background height in pixels.
     */
    public void updateBackgroundHeight(int height) {
        if (mSheetBackground == null) return;
        ViewGroup.LayoutParams bgParams = mSheetBackground.getLayoutParams();
        if (bgParams != null && bgParams.height != height) {
            bgParams.height = height;
            mSheetBackground.setLayoutParams(bgParams);
        }
    }
}

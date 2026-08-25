// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
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
    protected static final GlowSpec DEFAULT_GLOW_SPEC =
            new GlowSpec(0, GlowSpec.ShadowSize.DEFAULT);

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

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    protected TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /** The FrameLayout used to hold the bottom sheet toolbar. */
    protected TouchRestrictingFrameLayout mToolbarHolder;

    /** The view that contains the sheet background color. */
    protected View mSheetBackground;

    /** The view that contains the sheet background glow color. */
    protected View mShadowLayer;

    /**
     * The view that is used to the area below the bottom sheet contents that is normally obscured
     * by the keyboard.
     */
    protected View mKeyboardCurtain;

    /**
     * The optional 'X' close button. This is injected into large form factor layouts when the sheet
     * is non-modal (meaning it lacks a background scrim that would otherwise allow the user to
     * easily tap-to-dismiss).
     */
    protected @Nullable View mCloseButton;

    /**
     * An alternative shadow layer used exclusively on large form factor devices when the current
     * sheet content opts out of the new bottom sheet UI. This provides the standard mobile
     * bottom-edge bleeder shadow instead of the full perimeter rectangle shadow.
     */
    protected @Nullable View mFallbackShadowLayer;

    /** The drag handlebar view shown at the top of the sheet when requested by content. */
    protected ImageView mHandlebar;

    /** The shadow length in pixels for standard sheets. */
    protected final @Px int mShadowLength;

    /** The shadow length in pixels for large sheets. */
    protected final @Px int mShadowLengthLarge;

    /** The active visual layout mode of the sheet. */
    private @SheetLayoutMode int mLayoutMode = SheetLayoutMode.STANDARD;

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
     * Sets the elevation (Z-index) of the content container.
     *
     * @param z The elevation value in pixels.
     */
    public void setContainerZ(float z) {
        if (mBottomSheetContentContainer != null) {
            ViewCompat.setElevation(mBottomSheetContentContainer, z);
        }
    }

    /**
     * Sets whether the fallback shadow is visible.
     *
     * @param visible Whether the fallback shadow should be visible.
     */
    public void setFallbackShadowVisible(boolean visible) {
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
}

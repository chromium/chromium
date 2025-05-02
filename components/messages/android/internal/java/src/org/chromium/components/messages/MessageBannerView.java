// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.LayoutTransition;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Outline;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.SysUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost.PopupMenuShownListener;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.widget.ChromeImageButton;

/** View representing the message banner. */
@NullMarked
public class MessageBannerView extends RelativeLayout {
    private View mMessageBanner;
    private ImageView mIconView;
    private TextView mTitle;
    private TextViewWithCompoundDrawables mDescription;
    private @PrimaryWidgetAppearance int mPrimaryWidgetAppearance =
            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET;
    private TextView mPrimaryButton;
    private @Nullable String mPrimaryButtonText;
    private View mProgressIndicator;
    private ListMenuButton mSecondaryButton;
    private boolean mEnableCloseButton;
    private ChromeImageButton mCloseButton;
    private View mDivider;
    private @Nullable String mSecondaryButtonMenuText;
    private @Nullable Runnable mSecondaryActionCallback;
    private @Nullable ListMenuDelegate mSecondaryMenuButtonDelegate;
    private @Nullable SwipeGestureListener mSwipeGestureDetector;
    private @Nullable Runnable mOnTitleChanged;
    private int mCornerRadius = -1;
    private @Nullable PopupMenuShownListener mPopupMenuShownListener;
    private @Nullable Drawable mDescriptionDrawable;
    private boolean mOverrideSecondaryIconContentDescription = true;
    private @Nullable Supplier<Boolean> mIsWithinTapProtectionPeriodSupplier;

    public MessageBannerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMessageBanner = findViewById(R.id.message_banner);
        mTitle = findViewById(R.id.message_title);
        mDescription = findViewById(R.id.message_description);
        mPrimaryButton = findViewById(R.id.message_primary_button);
        mProgressIndicator = findViewById(R.id.message_primary_progress_indicator);
        mIconView = findViewById(R.id.message_icon);
        mSecondaryButton = findViewById(R.id.message_secondary_button);
        mCloseButton = findViewById(R.id.message_close_button);
        mDivider = findViewById(R.id.message_divider);
        mSecondaryButton.setOnClickListener(
                (View v) -> {
                    handleSecondaryButtonClick();
                });
        LinearLayout mainContent = findViewById(R.id.message_main_content);
        mainContent.getLayoutTransition().enableTransitionType(LayoutTransition.CHANGING);
        // Elevation does not work on low end device.
        if (SysUtils.isLowEndDevice()) {
            mMessageBanner.setBackground(
                    AppCompatResources.getDrawable(getContext(), R.drawable.dialog_bg_baseline));
        }
        int radius = getResources().getDimensionPixelOffset(R.dimen.message_close_button_size) / 2;
        mCloseButton.setOutlineProvider(new CloseButtonOutlineProvider(radius));
        mCloseButton.setClipToOutline(true);
    }

    void enableA11y(boolean enabled) {
        setImportantForAccessibility(
                enabled
                        ? IMPORTANT_FOR_ACCESSIBILITY_YES
                        : IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
    }

    void setTitle(String title) {
        mTitle.setText(title);
        if (mOnTitleChanged != null) mOnTitleChanged.run();
        if (mOverrideSecondaryIconContentDescription
                && TextUtils.isEmpty(mTitle.getContentDescription())) {
            setSecondaryIconContentDescription(
                    getResources().getString(R.string.message_more_options, title), true);
        }
    }

    void setTitleContentDescription(String description) {
        mTitle.setContentDescription(description);
        if (mOverrideSecondaryIconContentDescription) {
            setSecondaryIconContentDescription(
                    getResources().getString(R.string.message_more_options, description), true);
        }
    }

    void setDescriptionText(CharSequence description) {
        mDescription.setVisibility(TextUtils.isEmpty(description) ? GONE : VISIBLE);
        mDescription.setText(description);
    }

    void setDescriptionIcon(Drawable drawable) {
        mDescription.setVisibility(drawable == null ? GONE : VISIBLE);
        mDescriptionDrawable = drawable;
        mDescription.setDrawableTintColor(
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_secondary_tint_list));
        ((TextView) mDescription).setCompoundDrawablesRelative(drawable, null, null, null);
    }

    void enableDescriptionIconIntrinsicDimensions(boolean enabled) {
        if (mDescriptionDrawable != null) {
            int defaultIconSize =
                    getResources().getDimensionPixelOffset(R.dimen.message_description_icon_size);
            if (enabled) {
                int newWidth =
                        defaultIconSize
                                * mDescriptionDrawable.getIntrinsicWidth()
                                / mDescriptionDrawable.getIntrinsicHeight();
                mDescription.setDrawableWidth(newWidth);
            } else {
                mDescription.setDrawableWidth(defaultIconSize);
            }
            ((TextView) mDescription)
                    .setCompoundDrawablesRelative(mDescriptionDrawable, null, null, null);
        }
    }

    void setDescriptionMaxLines(int maxLines) {
        mDescription.setMaxLines(maxLines);
        mDescription.setEllipsize(TextUtils.TruncateAt.END);
    }

    void setIcon(Drawable icon) {
        mIconView.setImageDrawable(icon);
        // Reset radius to generate a new drawable with expected radius.
        if (mCornerRadius >= 0) setIconCornerRadius(mCornerRadius);
    }

    void setIconTint(@ColorInt int color) {
        if (color == MessageBannerProperties.TINT_NONE) {
            ImageViewCompat.setImageTintList(mIconView, null);
        } else {
            ImageViewCompat.setImageTintList(mIconView, ColorStateList.valueOf(color));
        }
    }

    void setIconCornerRadius(int cornerRadius) {
        mCornerRadius = cornerRadius;
        if (!(mIconView.getDrawable() instanceof BitmapDrawable)) {
            return;
        }
        BitmapDrawable drawable = (BitmapDrawable) mIconView.getDrawable();
        RoundedBitmapDrawable bitmap =
                ViewUtils.createRoundedBitmapDrawable(
                        getResources(), drawable.getBitmap(), cornerRadius);
        mIconView.setImageDrawable(bitmap);
    }

    void setPrimaryWidgetAppearance(@PrimaryWidgetAppearance int primaryWidgetAppearance) {
        mPrimaryWidgetAppearance = primaryWidgetAppearance;
        updatePrimaryWidgetAppearance();
    }

    void setPrimaryButtonText(String text) {
        mPrimaryButton.setText(text);
        mPrimaryButtonText = text;
        updatePrimaryWidgetAppearance();
    }

    void setPrimaryButtonTextMaxLines(int maxLines) {
        mPrimaryButton.setMaxLines(maxLines);
        updatePrimaryWidgetAppearance();
    }

    private void updatePrimaryWidgetAppearance() {
        boolean showSpinner = mPrimaryWidgetAppearance == PrimaryWidgetAppearance.PROGRESS_SPINNER;
        boolean showButton =
                mPrimaryWidgetAppearance == PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET
                        && !TextUtils.isEmpty(mPrimaryButtonText);
        assert !(showButton && showSpinner) : "Spinner and button cannot show at the same time. ";
        mProgressIndicator.setVisibility(showSpinner ? VISIBLE : GONE);
        mPrimaryButton.setVisibility(showButton ? VISIBLE : GONE);
    }

    void setPrimaryButtonClickListener(OnClickListener listener) {
        mPrimaryButton.setOnClickListener(
                (view) -> {
                    // Ignore click events if a progress bar is showing.
                    if (mPrimaryWidgetAppearance == PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET
                            && !TextUtils.isEmpty(mPrimaryButtonText)) {
                        listener.onClick(view);
                    }
                });
    }

    void enableCloseButton(boolean enable) {
        mEnableCloseButton = enable;
    }

    void setCloseButtonClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    void setSecondaryIcon(Drawable icon) {
        mSecondaryButton.setImageDrawable(icon);
        mSecondaryButton.setVisibility(VISIBLE);
        mDivider.setVisibility(VISIBLE);
    }

    void setSecondaryActionCallback(Runnable callback) {
        mSecondaryButton.dismiss();
        mSecondaryActionCallback = callback;
    }

    void setSecondaryButtonMenuText(String text) {
        mSecondaryButton.dismiss();
        mSecondaryButtonMenuText = text;
    }

    void setSecondaryMenuMaxSize(@SecondaryMenuMaxSize int maxSize) {
        int dimenId = R.dimen.message_secondary_menu_max_size_small;
        if (maxSize == SecondaryMenuMaxSize.MEDIUM) {
            dimenId = R.dimen.message_secondary_menu_max_size_medium;
        } else if (maxSize == SecondaryMenuMaxSize.LARGE) {
            dimenId = R.dimen.message_secondary_menu_max_size_large;
        }
        mSecondaryButton.setMenuMaxWidth(getResources().getDimensionPixelSize(dimenId));
    }

    void setSecondaryMenuButtonDelegate(ListMenuDelegate delegate) {
        mSecondaryButton.dismiss();
        mSecondaryMenuButtonDelegate = delegate;
    }

    void setSecondaryIconContentDescription(String description, boolean canBeOverridden) {
        mSecondaryButton.setContentDescription(description);
        mOverrideSecondaryIconContentDescription = canBeOverridden;
    }

    void setSwipeHandler(SwipeHandler handler) {
        mSwipeGestureDetector = new MessageSwipeGestureListener(getContext(), handler);
    }

    void setOnTitleChanged(@Nullable Runnable runnable) {
        mOnTitleChanged = runnable;
    }

    void setTapProtectionSupplier(Supplier<Boolean> supplier) {
        mIsWithinTapProtectionPeriodSupplier = supplier;
    }

    void dismissSecondaryMenuIfShown() {
        mSecondaryButton.dismiss();
    }

    void enableLargeIcon(boolean enabled) {
        int smallSize = getResources().getDimensionPixelSize(R.dimen.message_icon_size);
        int largeSize = getResources().getDimensionPixelSize(R.dimen.message_icon_size_large);
        ViewGroup.LayoutParams params = mIconView.getLayoutParams();
        if (enabled) {
            params.height = largeSize;
            params.width = largeSize;
        } else {
            params.width = LayoutParams.WRAP_CONTENT;
            params.height = smallSize;
        }
        mIconView.setLayoutParams(params);
    }

    void setPopupMenuShownListener(PopupMenuShownListener popupMenuShownListener) {
        mPopupMenuShownListener = popupMenuShownListener;
    }

    // TODO(crbug.com/40740070): For the M88 experiment we decided to display single item menu in
    // response to the tap on secondary button. The code below implements this logic. Past M88 it
    // will be replaced with modal dialog driven from the feature code.
    void handleSecondaryButtonClick() {
        if (mSecondaryMenuButtonDelegate == null && mSecondaryButtonMenuText == null) {
            if (mSecondaryActionCallback != null) {
                mSecondaryActionCallback.run();
            }
            return;
        }

        mSecondaryButton.setDelegate(
                mSecondaryMenuButtonDelegate != null
                        ? mSecondaryMenuButtonDelegate
                        : buildDelegateForSingleMenuItem());

        if (mPopupMenuShownListener != null) {
            mSecondaryButton.addPopupListener(mPopupMenuShownListener);
        }
        mSecondaryButton.showMenu();
    }

    void setMarginTop(int val) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) getLayoutParams();
        params.topMargin = val;
        setLayoutParams(params);
    }

    int getTitleHeightForAnimation() {
        return mTitle.getHeight();
    }

    int getTitleMeasuredHeightForAnimation() {
        return mTitle.getMeasuredHeight();
    }

    int getDescriptionHeightForAnimation() {
        return mDescription.getHeight();
    }

    int getDescriptionMeasuredHeightForAnimation() {
        return mDescription.getMeasuredHeight();
    }

    int getPrimaryButtonLineCountForAnimation() {
        return mPrimaryButton.getLineCount();
    }

    void resizeForStackingAnimation(
            int titleHeight, int descriptionHeight, int primaryButtonLineCount) {
        if (titleHeight != mTitle.getMeasuredHeight()) {
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) mTitle.getLayoutParams();
            params.height = titleHeight;
            mTitle.setLayoutParams(params);
        }

        if (descriptionHeight != mDescription.getMeasuredHeight()) {
            var params = (LinearLayout.LayoutParams) mDescription.getLayoutParams();
            params.height = descriptionHeight;
            mDescription.setLayoutParams(params);
        }

        mPrimaryButton.setMaxLines(primaryButtonLineCount);
        mPrimaryButton.setEllipsize(TextUtils.TruncateAt.END);
    }

    void resetForStackingAnimation() {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) mTitle.getLayoutParams();
        if (params.height != LayoutParams.WRAP_CONTENT) {
            params.height = LayoutParams.WRAP_CONTENT;
            mTitle.setLayoutParams(params);
        }

        params = (LinearLayout.LayoutParams) mDescription.getLayoutParams();
        if (params.height != LayoutParams.WRAP_CONTENT) {
            params.height = LayoutParams.WRAP_CONTENT;
            mDescription.setLayoutParams(params);
        }

        mPrimaryButton.setMaxLines(Integer.MAX_VALUE);
        mPrimaryButton.setEllipsize(null);
    }

    @Override
    public void setElevation(float elevation) {
        mMessageBanner.setElevation(elevation);
    }

    @Override
    protected boolean dispatchHoverEvent(MotionEvent event) {
        // TODO(crbug.com/315815559): pressing the button will trigger an unexpected exit, which
        // will make the close button disappear. Check #isPrimaryButton to prevent from that.
        // Remove the check once the fix lands.
        if (mEnableCloseButton && MotionEventUtils.isMouseEvent(event)) {
            if (event.getAction() == MotionEvent.ACTION_HOVER_ENTER) {
                mCloseButton.setVisibility(VISIBLE);
            } else if (event.getAction() == MotionEvent.ACTION_HOVER_EXIT
                    && MotionEventUtils.isPrimaryButton(event.getActionButton())) {
                mCloseButton.setVisibility(GONE);
            }
        }
        return super.dispatchHoverEvent(event);
    }

    /**
     * Overriding onMeasure for set a proper height for primary button. By design, the primary
     * button should fill all the remaining vertical space. If it includes very long text which
     * makes its height larger than the main content (title + description), we should manually
     * increase its height to prevent its text from being clipped.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mPrimaryButton.setMinHeight(0); // Reset min height for measuring.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int containerHeight = mMessageBanner.getMeasuredHeight();
        int btnWidth = mPrimaryButton.getMeasuredWidth();
        int wSpec = MeasureSpec.makeMeasureSpec(btnWidth, MeasureSpec.EXACTLY);
        int hSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mPrimaryButton.measure(wSpec, hSpec);
        int measuredHeight = mPrimaryButton.getMeasuredHeight();
        mPrimaryButton.setMinHeight(Math.max(measuredHeight, containerHeight));
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private ListMenuDelegate buildDelegateForSingleMenuItem() {
        assumeNonNull(mSecondaryButtonMenuText);
        MVCListAdapter.ListItem listItem =
                BrowserUiListMenuUtils.buildMenuListItem(mSecondaryButtonMenuText, 0, 0, true);
        MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
        menuItems.add(listItem);

        ListMenu.Delegate listMenuDelegate =
                (PropertyModel menuItem) -> {
                    assert menuItem == listItem.model;
                    // There is only one menu item in the menu.
                    if (mSecondaryActionCallback != null) {
                        mSecondaryActionCallback.run();
                    }
                };
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(getContext(), menuItems, listMenuDelegate);

        return new ListMenuDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }
        };
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return isWithinTapProtectionPeriod() || super.onInterceptTouchEvent(ev);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (isWithinTapProtectionPeriod()) return true;
        if (mSwipeGestureDetector != null) {
            return mSwipeGestureDetector.onTouchEvent(event) || super.onTouchEvent(event);
        }
        return super.onTouchEvent(event);
    }

    private boolean isWithinTapProtectionPeriod() {
        return mIsWithinTapProtectionPeriodSupplier != null
                && Boolean.TRUE.equals(mIsWithinTapProtectionPeriodSupplier.get());
    }

    private static class MessageSwipeGestureListener extends SwipeGestureListener {
        public MessageSwipeGestureListener(Context context, SwipeHandler handler) {
            super(context, handler);
        }

        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }
    }

    // A circular view without shadow.
    private static class CloseButtonOutlineProvider extends RoundedCornerOutlineProvider {
        public CloseButtonOutlineProvider(int radius) {
            super(radius);
        }

        @Override
        public void getOutline(View view, Outline outline) {
            super.getOutline(view, outline);
            outline.setAlpha(0f);
        }
    }

    ListMenuButton getSecondaryButtonForTesting() {
        return mSecondaryButton;
    }

    float getElevationForTesting() {
        return mMessageBanner.getElevation();
    }

    public View getMainContentForTesting() {
        return findViewById(R.id.message_banner);
    }
}

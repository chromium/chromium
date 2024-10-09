// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.ScaleAnimation;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

/** A container class for a view showing a photo in the Photo Picker. */
public class PickerBitmapView extends SelectableItemViewBase<PickerBitmap> {
    // The length of the image selection animation (in ms).
    private static final int ANIMATION_DURATION = 100;

    // The length of the fade in animation (in ms).
    private static final int IMAGE_FADE_IN_DURATION = 200;

    // The length of the image frame display (in ms).
    private static final int IMAGE_FRAME_DISPLAY = 250;

    // An animation listener to verify correct selection behavior with tests.
    private static AnimationListener sAnimationListenerForTest;

    // Our context.
    private Context mContext;

    // Our parent category.
    private PickerCategoryView mCategoryView;

    // Our selection delegate.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The request details (meta-data) for the bitmap shown.
    private PickerBitmap mBitmapDetails;

    // The image view containing the bitmap.
    private ImageView mIconView;

    // The aspect ratio of the image (>1.0=portrait, <1.0=landscape, invalid if -1).
    private float mRatio = -1;

    // The container for the small version of the video UI (duration and small play button).
    private ViewGroup mVideoControlsSmall;

    // For video tiles, this lists the duration of the video. Blank for other types.
    private TextView mVideoDuration;

    // The Play button in the top right corner. Only shown for videos.
    private ImageView mPlayButton;

    // The large Play button (in the middle when in full-screen mode). Only shown for videos.
    private ImageView mPlayButtonLarge;

    // The little shader in the top left corner (provides backdrop for selection ring on
    // unfavorable image backgrounds).
    private ImageView mScrim;

    // The control that signifies the image has been selected.
    private ImageView mSelectedView;

    // The control that signifies the image has not been selected.
    private ImageView mUnselectedView;

    // The camera/gallery special tile (with icon as drawable).
    private View mSpecialTile;

    // The camera/gallery icon.
    public ImageView mSpecialTileIcon;

    // The label under the special tile.
    public TextView mSpecialTileLabel;

    // Whether the image has been loaded already.
    private boolean mImageLoaded;

    // The background color to use for the tile (either the special tile or for pictures, when not
    // animating).
    private int mBackgroundColor = Color.TRANSPARENT;

    // The selected state of a given picture tile.
    private boolean mSelectedState;

    /** Constructor for inflating from XML. */
    public PickerBitmapView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @SuppressWarnings("WrongViewCast") // Android lint gets confused: https://crbug.com/1315709
    private void assignScrim() {
        mScrim = findViewById(R.id.scrim);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        assignScrim();
        mIconView = findViewById(R.id.bitmap_view);
        mSelectedView = findViewById(R.id.selected);
        mUnselectedView = findViewById(R.id.unselected);
        mSpecialTile = findViewById(R.id.special_tile);
        mSpecialTileIcon = findViewById(R.id.special_tile_icon);
        mSpecialTileLabel = findViewById(R.id.special_tile_label);

        // Specific UI controls for video support.
        mVideoControlsSmall = findViewById(R.id.video_controls_small);
        mVideoDuration = findViewById(R.id.video_duration);
        mPlayButton = findViewById(R.id.small_play_button);
        mPlayButton.setOnClickListener(this);
        mPlayButtonLarge = findViewById(R.id.large_play_button);
        mPlayButtonLarge.setOnClickListener(this);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mCategoryView == null) return;

        if (mCategoryView.isInMagnifyingMode()) {
            int height;
            if (isPictureTile()) {
                // Use ratio to determine how tall the image wants to be.
                height = (int) (mRatio * mCategoryView.getImageWidth());
            } else {
                // Special tiles have a fixed height in magnifying mode.
                height = mCategoryView.getSpecialTileHeight();
            }

            setMeasuredDimension(mCategoryView.getImageWidth(), height);
        } else {
            // Use small thumbnails that are square in size.
            setMeasuredDimension(mCategoryView.getImageWidth(), mCategoryView.getImageWidth());
        }
    }

    @Override
    public final void onClick(View view) {
        if (view == mPlayButton || view == mPlayButtonLarge) {
            mCategoryView.startVideoPlaybackAsync(mBitmapDetails.getUri());
        } else {
            super.onClick(view);
        }
    }

    @Override
    public void handleNonSelectionClick() {
        if (mBitmapDetails == null) {
            return; // Clicks are disabled until initialize() has been called.
        }

        if (isGalleryTile()) {
            mCategoryView.showGallery();
            return;
        } else if (isCameraTile()) {
            mCategoryView.showCamera();
            return;
        }

        // The SelectableItemView expects long press to be the selection event, but this class wants
        // that to happen on click instead.
        onLongClick(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // When views get detached, they become unselected. By reflecting that here, we can make
        // sure they get re-selected when they scroll out of view and avoid unnecessary updates to
        // views that are in the correct state already.
        mSelectedState = false;
    }

    @Override
    protected boolean toggleSelectionForItem(PickerBitmap item) {
        if (isGalleryTile() || isCameraTile()) return false;
        if (mCategoryView.isZoomSwitchingInEffect()) return false;
        return super.toggleSelectionForItem(item);
    }

    @Override
    public void setChecked(boolean checked) {
        if (!isPictureTile()) {
            return;
        }

        super.setChecked(checked);
        updateSelectionState(/* animateBorderChanges= */ false);
    }

    @Override
    public void onSelectionStateChange(List<PickerBitmap> selectedItems) {
        // If the user cancels the dialog before this object has initialized,
        // the SelectionDelegate will try to notify us that all selections have
        // been cleared. However, we don't need to process that message and, in
        // fact, we can't do so because isPictureTile relies on mBitmapDetails
        // being initialized.
        if (mBitmapDetails == null) return;

        boolean animateBorderChanges = selectedItems.contains(mBitmapDetails) != super.isChecked();
        updateSelectionState(animateBorderChanges);

        super.onSelectionStateChange(selectedItems);
    }

    private void updateSelectionBorder(boolean animate) {
        if (!isAttachedToWindow()) {
            // No need to update something that's not attached to a window. As soon as the view is
            // re-attached, it will be updated.
            return;
        }

        // TODO(finnur): Look into whether using #updateView can simplify this logic.
        boolean selected = mSelectionDelegate.isItemSelected(mBitmapDetails);

        // Selection border is not in use when in magnifying mode. Selected bitmaps will still have
        // a checkmark, indicating selection.
        if (mCategoryView.isInMagnifyingMode()) {
            selected = false;
        }

        if (selected == mSelectedState) {
            // No need to change to a state that is already set.
            return;
        }

        mSelectedState = selected;

        float startX;
        float endX;
        float startY;
        float endY;
        float videoDurationOffsetX;
        float videoDurationOffsetY;

        float endStateWhenSelectedX = 0.8f; // 20% border around small thumbnails.
        float endStateWhenSelectedY = 0.8f;
        if (mCategoryView.isInMagnifyingMode()) {
            // Ratio above 1.0 is portrait, and less than 1.0 is landscape. With full screen
            // images, the sides are uneven in length, so the percentage used for the border
            // should adjust to a difference in length. Left and right border use up 8% of total
            // width available. Top and bottom border are then calculated to be the same size.
            float width = mCategoryView.getImageWidth();
            float percentage = 0.92f;
            float borderSizePixels = width * (1.0f - percentage);
            endStateWhenSelectedX = percentage;
            endStateWhenSelectedY = 1.0f - (borderSizePixels / (width * mRatio));
        }

        if (selected) {
            startX = 1f;
            startY = 1f;
            endX = endStateWhenSelectedX;
            endY = endStateWhenSelectedY;

            float pixels =
                    getResources()
                            .getDimensionPixelSize(R.dimen.photo_picker_video_duration_offset);
            videoDurationOffsetX = -pixels;
            videoDurationOffsetY = pixels;
        } else {
            startX = endStateWhenSelectedX;
            startY = endStateWhenSelectedY;
            endX = 1f;
            endY = 1f;

            videoDurationOffsetX = 0;
            videoDurationOffsetY = 0;
        }

        Animation animation =
                new ScaleAnimation(
                        startX,
                        endX, // Values for x axis.
                        startY,
                        endY, // Values for y axis.
                        Animation.RELATIVE_TO_SELF,
                        0.5f, // Pivot X-axis type and value.
                        Animation.RELATIVE_TO_SELF,
                        0.5f); // Pivot Y-axis type and value.
        animation.setDuration(animate ? ANIMATION_DURATION : 0);
        animation.setFillAfter(true); // Keep the results of the animation.
        if (sAnimationListenerForTest != null) {
            animation.setAnimationListener(sAnimationListenerForTest);
        }
        mIconView.startAnimation(animation);

        ObjectAnimator videoDurationX =
                ObjectAnimator.ofFloat(
                        mVideoControlsSmall, View.TRANSLATION_X, videoDurationOffsetX);
        ObjectAnimator videoDurationY =
                ObjectAnimator.ofFloat(
                        mVideoControlsSmall, View.TRANSLATION_Y, videoDurationOffsetY);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(videoDurationX, videoDurationY);
        animatorSet.setDuration(animate ? ANIMATION_DURATION : 0);
        animatorSet.start();
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        if (!isPictureTile()) return;

        info.setCheckable(true);
        info.setChecked(isChecked());
        CharSequence text =
                mBitmapDetails.getFilenameWithoutExtension()
                        + " "
                        + mBitmapDetails.getLastModifiedString();
        info.setText(text);
    }

    /**
     * Sets the {@link PickerCategoryView} for this PickerBitmapView.
     *
     * @param categoryView The category view showing the images. Used to access common functionality
     *     and sizes and retrieve the {@link SelectionDelegate}.
     */
    public void setCategoryView(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
        mSelectionDelegate = mCategoryView.getSelectionDelegate();
        setSelectionDelegate(mSelectionDelegate);
    }

    /**
     * Completes the initialization of the PickerBitmapView. Must be called before the image can
     * respond to click events.
     *
     * @param bitmapDetails The details about the bitmap represented by this PickerBitmapView.
     * @param thumbnails The Bitmaps to use for the thumbnail (or null).
     * @param videoDuration The time-length of the video (human-friendly string).
     * @param placeholder Whether the image given is a placeholder or the actual image.
     * @param ratio The aspect ratio of the image, if it were shown full-width.
     */
    public void initialize(
            PickerBitmap bitmapDetails,
            @Nullable List<Bitmap> thumbnails,
            String videoDuration,
            boolean placeholder,
            float ratio) {
        resetTile();

        mBitmapDetails = bitmapDetails;
        setItem(bitmapDetails);
        if (isCameraTile() || isGalleryTile()) {
            initializeSpecialTile();
            mImageLoaded = true;
        } else {
            setThumbnailBitmap(thumbnails, videoDuration, ratio);
            mImageLoaded = !placeholder;
        }

        updateSelectionState(/* animateBorderChanges= */ false);
    }

    /** Initialization for the special tiles (camera/gallery icon). */
    public void initializeSpecialTile() {
        int labelStringId = 0;
        Drawable image = null;
        Resources resources = mContext.getResources();

        if (isCameraTile()) {
            image =
                    TraceEventVectorDrawableCompat.create(
                            resources, R.drawable.ic_photo_camera_grey, mContext.getTheme());
            labelStringId = R.string.photo_picker_camera;
        } else if (isGalleryTile()) {
            image =
                    TraceEventVectorDrawableCompat.create(
                            resources, R.drawable.ic_collections_grey, mContext.getTheme());
            labelStringId = R.string.photo_picker_browse;
        } else {
            assert false;
        }

        mSpecialTileIcon.setImageDrawable(image);
        ImageViewCompat.setImageTintList(
                mSpecialTileIcon,
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_secondary_tint_list));
        ImageViewCompat.setImageTintMode(mSpecialTileIcon, PorterDuff.Mode.SRC_IN);
        mSpecialTileLabel.setText(labelStringId);

        // Reset visibility, since #initialize() sets mSpecialTile visibility to GONE.
        mSpecialTile.setVisibility(View.VISIBLE);
        mSpecialTileIcon.setVisibility(View.VISIBLE);
        mSpecialTileLabel.setVisibility(View.VISIBLE);
    }

    /**
     * Sets a thumbnail bitmap for the current view and ensures the selection border is showing, if
     * the image has already been selected.
     *
     * @param thumbnails The Bitmaps to use for the icon ImageView.
     * @param videoDuration The time-length of the video (human-friendly string).
     * @param ratio The aspect ratio of the image, if it were shown using the full screen width.
     * @return True if no image was loaded before (e.g. not even a low-res image).
     */
    public boolean setThumbnailBitmap(List<Bitmap> thumbnails, String videoDuration, float ratio) {
        assert thumbnails == null || thumbnails.size() > 0;

        // There are four cases to consider:
        // 1) When placeholders are assigned, thumbnails=null and videoDuration=null.
        // 2) When images are shown, videoDuration is null and thumbnail size is 1.
        // 3) Videos: one thumbnail is shown first (videoDuration non-null, thumbnail.size() = 1).
        // 4) Then, as more video frames are decoded (thumbnail.size() > 1).
        // Only the last case needs to branch into the AnimationDrawable part.
        if (videoDuration == null || thumbnails.size() == 1) {
            mIconView.setImageBitmap(thumbnails == null ? null : thumbnails.get(0));
        } else {
            final AnimationDrawable animationDrawable = new AnimationDrawable();
            for (int i = 0; i < thumbnails.size(); ++i) {
                animationDrawable.addFrame(
                        new BitmapDrawable(mContext.getResources(), thumbnails.get(i)),
                        IMAGE_FRAME_DISPLAY);
            }
            animationDrawable.setOneShot(false);
            mIconView.setImageDrawable(animationDrawable);
            animationDrawable.start();
        }
        mVideoDuration.setText(videoDuration);

        if (thumbnails != null && thumbnails.size() > 0) {
            mRatio = ratio;
        }

        boolean noImageWasLoaded = !mImageLoaded;
        mImageLoaded = true;
        updateSelectionState(/* animateBorderChanges= */ false);

        return noImageWasLoaded;
    }

    /**
     * Initiates fading in of the thumbnail. Note, this should not be called if a grainy version of
     * the thumbnail was loaded from cache. Otherwise a flash will appear.
     */
    public void fadeInThumbnail() {
        mIconView.setAlpha(0.0f);
        mIconView.animate().alpha(1.0f).setDuration(IMAGE_FADE_IN_DURATION).start();
    }

    /**
     * Resets the view to its starting state, which is necessary when the view is about to be
     * re-used.
     */
    private void resetTile() {
        mBitmapDetails = null;
        mIconView.setImageBitmap(null);
        mPlayButtonLarge.setVisibility(View.GONE);
        mVideoDuration.setText("");
        mVideoControlsSmall.setVisibility(View.GONE);
        mUnselectedView.setVisibility(View.GONE);
        mSelectedView.setVisibility(View.GONE);
        mScrim.setVisibility(View.GONE);
        mSpecialTile.setVisibility(View.GONE);
        mSpecialTileIcon.setVisibility(View.GONE);
        mSpecialTileLabel.setVisibility(View.GONE);
        mSelectedState = false;
        setEnabled(true);
    }

    /** Updates the selection controls for this view. */
    private void updateSelectionState(boolean animateBorderChanges) {
        boolean special = !isPictureTile();
        boolean anySelection =
                mSelectionDelegate != null && mSelectionDelegate.isSelectionEnabled();
        int bgColorId;
        if (!special) {
            bgColorId = R.color.photo_picker_tile_bg_color;
        } else {
            bgColorId = R.color.photo_picker_special_tile_bg_color;
            mSpecialTileLabel.setEnabled(!anySelection);
            mSpecialTileIcon.setEnabled(!anySelection);
            setEnabled(!anySelection);
        }

        mBackgroundColor = mContext.getColor(bgColorId);
        setBackgroundColor(
                mCategoryView.isZoomSwitchingInEffect() && !special
                        ? Color.TRANSPARENT
                        : mBackgroundColor);

        boolean isSelected = mSelectionDelegate.isItemSelected(mBitmapDetails);
        mSelectedView.setVisibility(!special && isSelected ? View.VISIBLE : View.GONE);
        // The visibility of the unselected toggle for multi-selection mode is a little more complex
        // because we don't want to show it when nothing is selected (unless in magnifying mode) and
        // also not on a blank canvas.
        boolean showUnselectedToggle =
                !special
                        && !isSelected
                        && mImageLoaded
                        && (anySelection || mCategoryView.isInMagnifyingMode())
                        && mCategoryView.isMultiSelectAllowed();
        mUnselectedView.setVisibility(showUnselectedToggle ? View.VISIBLE : View.GONE);
        mScrim.setVisibility(showUnselectedToggle ? View.VISIBLE : View.GONE);

        boolean showVideoControls =
                mImageLoaded && mBitmapDetails.type() == PickerBitmap.TileTypes.VIDEO;
        mVideoControlsSmall.setVisibility(
                showVideoControls && !mCategoryView.isInMagnifyingMode()
                        ? View.VISIBLE
                        : View.GONE);
        mPlayButtonLarge.setVisibility(
                showVideoControls && mCategoryView.isInMagnifyingMode() ? View.VISIBLE : View.GONE);

        if (!special) {
            updateSelectionBorder(animateBorderChanges);
        }
    }

    private boolean isGalleryTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.GALLERY;
    }

    private boolean isCameraTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.CAMERA;
    }

    private boolean isPictureTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.PICTURE
                || mBitmapDetails.type() == PickerBitmap.TileTypes.VIDEO;
    }

    public static void setAnimationListenerForTest(AnimationListener listener) {
        sAnimationListenerForTest = listener;
        ResettersForTesting.register(() -> sAnimationListenerForTest = null);
    }
}

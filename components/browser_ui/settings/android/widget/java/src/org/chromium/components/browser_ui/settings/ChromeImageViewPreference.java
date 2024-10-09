// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

/**
 * A preference that supports some Chrome-specific customizations:
 *
 * 1. This preference supports being managed. If this preference is managed (as determined by its
 *    ManagedPreferenceDelegate), it updates its appearance and behavior appropriately: shows an
 *    enterprise icon in its widget ImageView, disables clicks, etc.
 * 2. This preference can have a multiline title.
 * 3. This preference can have an onClick listener set for its ImageView widget.
 *
 * The preference includes the preference_chrome_image_view widget layout to provide these
 * customizations, however a custom widget may also be included as long as there is an ImageView
 * with the image_view_widget ID.
 */
public class ChromeImageViewPreference extends Preference {
    @Nullable private ManagedPreferenceDelegate mManagedPrefDelegate;

    /** The onClick listener to handle click events for the ImageView widget. */
    @Nullable private View.OnClickListener mListener;

    /** The image resource ID to use for the ImageView widget source. */
    @DrawableRes private int mImageRes;

    /** The color resource ID for tinting of ImageView widget. */
    @ColorRes private int mColorRes;

    /** The color resource ID for tinting of the view's background. */
    @ColorRes private Integer mBackgroundColorRes;

    /** The string to use for the ImageView widget content description. */
    private CharSequence mContentDescription;

    /** Whether the ImageView should be enabled. */
    private boolean mImageViewEnabled = true;

    /** The ImageView Button. */
    private ImageView mButton;

    /** The View for this preference. */
    private View mView;

    /** The ints to set the ImageView padding. */
    private int mImageViewLeftPadding;

    private int mImageViewTopPadding;
    private int mImageViewBottomPadding;
    private int mImageViewRightPadding;

    /** Whether the ImageView has any custom padding set. False by default. */
    private boolean mImageViewCustomPadding;

    /** Constructor for use in Java. */
    public ChromeImageViewPreference(Context context) {
        this(context, null);
        // Set custom padding to false until user calls setImagePadding().
        mImageViewCustomPadding = false;
    }

    /** Constructor for inflating from XML. */
    public ChromeImageViewPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setWidgetLayoutResource(R.layout.preference_chrome_image_view);
        setSingleLineTitle(false);
        setImageColor(R.color.default_icon_color_tint_list);
    }

    /** Sets the ManagedPreferenceDelegate which will determine whether this preference is managed. */
    public void setManagedPreferenceDelegate(@Nullable ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ false,
                /* hasCustomLayout= */ true);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mButton = (ImageView) holder.findViewById(R.id.image_view_widget);
        mButton.setBackgroundColor(Color.TRANSPARENT);
        mButton.setVisibility(View.VISIBLE);

        mView = holder.itemView;
        updateBackground();
        configureImageView();
        ManagedPreferencesUtils.onBindViewToImageViewPreference(mManagedPrefDelegate, this, mView);
    }

    @Override
    protected void onClick() {
        if (ManagedPreferencesUtils.onClickPreference(mManagedPrefDelegate, this)) return;
        super.onClick();
    }

    /**
     * Sets the Drawable resource ID, the String, and the OnClickListener for the ImageView widget's
     * source, content description, and onClick, respectively. Passing 0 as the Drawable resource ID
     * will reset the image and related attributes to their default value.
     */
    public void setImageView(
            @DrawableRes int imageRes,
            CharSequence contentDescription,
            @Nullable View.OnClickListener listener) {
        mImageRes = imageRes;
        mContentDescription = contentDescription;
        mListener = listener;
        configureImageView();
        notifyChanged();
    }

    /**
     * Sets the Drawable resource ID, the String resource ID, and the OnClickListener for the
     * ImageView widget's source, content description, and onClick, respectively. Passing 0 as the
     * Drawable resource ID will reset the image and related attributes to their default value.
     */
    public void setImageView(
            @DrawableRes int imageRes,
            @StringRes int contentDescriptionRes,
            @Nullable View.OnClickListener listener) {
        setImageView(
                imageRes,
                (contentDescriptionRes != 0) ? getContext().getString(contentDescriptionRes) : null,
                listener);
    }

    /** Sets the Color resource ID which will be used to set the color of the image. */
    public void setImageColor(@ColorRes int colorRes) {
        if (mColorRes == colorRes) return;

        mColorRes = colorRes;
        configureImageView();
    }

    /** Sets the the padding of the ImageView. */
    public void setImagePadding(int left, int top, int right, int bottom) {
        mImageViewLeftPadding = left;
        mImageViewTopPadding = top;
        mImageViewRightPadding = right;
        mImageViewBottomPadding = bottom;
        mImageViewCustomPadding = true;
    }

    /** Sets the Color resource ID which will be used to set the color of the view. */
    public void setBackgroundColor(@ColorRes int colorRes) {
        if (mBackgroundColorRes != null && mBackgroundColorRes == colorRes) return;
        mBackgroundColorRes = colorRes;
        updateBackground();
    }

    /** Enables/Disables the ImageView, allowing for clicks to pass through (when disabled). */
    public void setImageViewEnabled(boolean enabled) {
        if (mImageViewEnabled == enabled) return;

        mImageViewEnabled = enabled;
        configureImageView();
    }

    /**
     * Enables/Disables whether the preference (row) view is clickable or not. Currently used for
     * Zoom site setting.
     */
    public void setViewClickable(boolean enabled) {
        mView.setClickable(enabled);
    }

    /**
     * If a {@link ManagedPreferenceDelegate} has been set, check if this preference is managed.
     * @return True if the preference is managed.
     */
    public boolean isManaged() {
        if (mManagedPrefDelegate == null) return false;

        return mManagedPrefDelegate.isPreferenceControlledByPolicy(this)
                || mManagedPrefDelegate.isPreferenceControlledByCustodian(this);
    }

    private void configureImageView() {
        if (mButton == null) {
            return;
        }

        if (mImageRes == 0) {
            // Reset to default behavior. Especially useful for |onBindViewHolder|, so the |holder|
            // doesn't inherit wrong recycled buttons.
            mButton.setImageDrawable(null);
            mButton.setPadding(0, 0, 0, 0);
            mButton.setOnClickListener(null);
            mButton.setContentDescription(null);
            mButton.setClickable(false);
            return;
        }

        Drawable buttonImg = SettingsUtils.getTintedIcon(getContext(), mImageRes, mColorRes);
        mButton.setImageDrawable(buttonImg);
        mButton.setEnabled(mImageViewEnabled);
        if (mImageViewCustomPadding) {
            mButton.setPadding(
                    mImageViewLeftPadding,
                    mImageViewTopPadding,
                    mImageViewRightPadding,
                    mImageViewBottomPadding);
        }
        if (mImageViewEnabled) mButton.setOnClickListener(mListener);

        if (mContentDescription != null) {
            mButton.setContentDescription(mContentDescription);
        }
    }

    private void updateBackground() {
        if (mView == null || mBackgroundColorRes == null) return;
        mView.setBackgroundColor(
                AppCompatResources.getColorStateList(getContext(), mBackgroundColorRes)
                        .getDefaultColor());
    }

    @VisibleForTesting
    public ImageView getButton() {
        return mButton;
    }
}

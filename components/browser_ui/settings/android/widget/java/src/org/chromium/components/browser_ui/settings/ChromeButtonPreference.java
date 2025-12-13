// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A preference that supports some Chrome-specific customizations:
 *
 * <p>1. This preference supports being managed. If this preference is managed (as determined by its
 * ManagedPreferenceDelegate), it updates its appearance and behavior appropriately: shows an
 * enterprise icon in its widget Button, disables clicks, etc. 2. This preference can have a
 * multiline title. 3. This preference can have an onClick listener set for its Button widget.
 *
 * <p>The preference includes the preference_chrome_button widget layout to provide these
 * customizations, however a custom widget may also be included as long as there is an Button with
 * the button_widget ID.
 */
@NullMarked
public class ChromeButtonPreference extends ChromeBasePreference {
    /** The onClick listener to handle click events for the Button widget. */
    private View.@Nullable OnClickListener mListener;

    /** The text to use for the Button widget source. */
    private @Nullable CharSequence mText;

    /** The color resource ID for tinting of the view's background. */
    @ColorRes private @Nullable Integer mBackgroundColorRes;

    /** The string to use for the Button widget content description. */
    private @Nullable CharSequence mContentDescription;

    /** Whether the Button should be enabled. */
    private boolean mButtonEnabled = true;

    /** The Button Button. */
    private @Nullable Button mButton;

    /** The View for this preference. */
    private @Nullable View mView;

    /** Constructor for use in Java. */
    public ChromeButtonPreference(Context context) {
        this(context, null);
    }

    /** Constructor for inflating from XML. */
    public ChromeButtonPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        setWidgetLayoutResource(R.layout.preference_chrome_button);
    }

    @Override
    public void setManagedPreferenceDelegate(@Nullable ManagedPreferenceDelegate delegate) {
        super.setManagedPreferenceDelegate(
                delegate, /* allowManagedIcon= */ false, /* hasCustomLayout= */ true);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mButton = (Button) holder.findViewById(R.id.button_widget);

        mView = holder.itemView;
        updateBackground();
        configureButton();
    }


    /**
     * Sets the Text, the String, and the OnClickListener for the Button widget's source, content
     * description, and onClick, respectively. Passing null as the Text will reset the text and
     * related attributes to their default value.
     */
    public void setButton(
            @Nullable CharSequence text,
            @Nullable CharSequence contentDescription,
            View.@Nullable OnClickListener listener) {
        mText = text;
        mContentDescription = contentDescription;
        mListener = listener;
        configureButton();
        notifyChanged();
    }

    /**
     * Sets the Text resource ID, the String resource ID, and the OnClickListener for the Button
     * widget's source, content description, and onClick, respectively. Passing 0 as the Text
     * resource ID will reset the text and related attributes to their default value.
     */
    public void setButton(
            @StringRes int textRes,
            @StringRes int contentDescriptionRes,
            View.@Nullable OnClickListener listener) {
        setButton(
                (textRes != 0) ? getContext().getString(textRes) : null,
                (contentDescriptionRes != 0) ? getContext().getString(contentDescriptionRes) : null,
                listener);
    }

    /** Sets the Color resource ID which will be used to set the color of the view. */
    public void setBackgroundColor(@ColorRes int colorRes) {
        if (mBackgroundColorRes != null && mBackgroundColorRes == colorRes) return;
        mBackgroundColorRes = colorRes;
        updateBackground();
    }

    /** Enables/Disables the Button, allowing for clicks to pass through (when disabled). */
    public void setButtonEnabled(boolean enabled) {
        if (mButtonEnabled == enabled) return;

        mButtonEnabled = enabled;
        configureButton();
    }


    private void configureButton() {
        if (mButton == null || mText == null) {
            return;
        }

        mButton.setText(mText);
        mButton.setEnabled(mButtonEnabled);

        if (mButtonEnabled) {
            mButton.setOnClickListener(mListener);
        }

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
    public @Nullable Button getButton() {
        return mButton;
    }
}

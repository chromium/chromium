// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.widget.containment.ContainmentUiUtils.parseContainmentAttributes;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.containment.ContainmentUiUtils;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * A preference wrapper for {@link MaterialCardViewNoShadow} with an icon, a text message and an
 * optional close button.
 */
@NullMarked
public class CardPreference extends TextMessagePreference {
    private @Nullable CharSequence mSummary;
    private @Nullable Drawable mIconDrawable;
    private int mCloseIconVisibility;
    private @Nullable OnClickListener mOnCloseClickListener;

    private @Nullable TextViewWithClickableSpans mDescriptionView;
    private @Nullable ChromeImageView mIcon;
    private @Nullable ChromeImageView mCloseIcon;
    private boolean mShouldCenterIcon;
    private final int mBackgroundStyle;
    private final int mBackgroundColor;

    /** Constructor for inflating from XML. */
    public CardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.card_preference);
        setSelectable(false);

        ContainmentUiUtils.ContainmentAttributes containmentAttributes =
                parseContainmentAttributes(context, attrs);
        mBackgroundStyle = containmentAttributes.backgroundStyle;
        mBackgroundColor = containmentAttributes.backgroundColor;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mDescriptionView = (TextViewWithClickableSpans) holder.findViewById(R.id.summary);
        mIcon = (ChromeImageView) holder.findViewById(R.id.icon);
        mCloseIcon = (ChromeImageView) holder.findViewById(R.id.close_icon);

        mDescriptionView.setText(mSummary);
        ClickableSpan[] spans = mDescriptionView.getClickableSpans();
        // Set the movement method, only if there is an interactive element. This avoids the element
        // being keyboard focusable if there isn't any focusable element.
        if (spans != null && spans.length > 0) {
            mDescriptionView.setMovementMethod(LinkMovementMethod.getInstance());
        }

        mIcon.setImageDrawable(mIconDrawable);
        if (mShouldCenterIcon) {
            LinearLayout.LayoutParams iconParams =
                    (LinearLayout.LayoutParams) mIcon.getLayoutParams();
            iconParams.gravity = Gravity.CENTER_VERTICAL;
            mIcon.setLayoutParams(iconParams);
        }

        mCloseIcon.setVisibility(mCloseIconVisibility);
        mCloseIcon.setOnClickListener(mOnCloseClickListener);

        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setTextAppearance(R.style.TextAppearance_Headline2Thick);
    }

    /**
     * Set card summary.
     *
     * @param summary Summary char sequence.
     */
    @Override
    public void setSummary(@Nullable CharSequence summary) {
        mSummary = summary;
    }

    /**
     * Set card icon drawable.
     *
     * @param iconDrawable Drawable to be shown.
     */
    public void setIconDrawable(Drawable iconDrawable) {
        mIconDrawable = iconDrawable;
    }

    /**
     * Set close icon visibility.
     *
     * @param visibility Close icon visibility.
     */
    public void setCloseIconVisibility(int visibility) {
        mCloseIconVisibility = visibility;
    }

    /**
     * Set on close click listener.
     *
     * @param onCloseClickListener The close icon click listener.
     */
    public void setOnCloseClickListener(OnClickListener onCloseClickListener) {
        this.mOnCloseClickListener = onCloseClickListener;
    }

    /**
     * @param shouldCenterIcon Whether to center the left icon vertically inside the card.
     */
    public void setShouldCenterIcon(boolean shouldCenterIcon) {
        mShouldCenterIcon = shouldCenterIcon;
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return mBackgroundStyle;
    }

    @Override
    public int getCustomBackgroundColor() {
        return mBackgroundColor;
    }
}

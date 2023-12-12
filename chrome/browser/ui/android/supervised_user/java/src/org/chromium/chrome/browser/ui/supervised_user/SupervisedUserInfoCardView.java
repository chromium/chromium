// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.supervised_user;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.MaterialCardViewNoShadow;
import org.chromium.ui.text.SpanApplier;

/**
 * Container view for InfoCard shown to supervised users when they use the discover feed for the
 * first time.
 */
public class SupervisedUserInfoCardView extends FrameLayout {
    private TextView mTitle;
    private Context mContext;
    private TextView mDescription;

    private ImageButton mDismissButton;
    private MaterialCardViewNoShadow mMaterialCardViewNoShadow;

    /** Constructor for when the SupervisedUserInfoCardView is inflated from XML. */
    public SupervisedUserInfoCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.supervised_user_discover_info_card, this);
        mContext = context;
        mMaterialCardViewNoShadow = findViewById(R.id.discover_info_view_wrapper);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.discover_info_card_title);
        mDescription = findViewById(R.id.discover_info_card_description);
        mDismissButton = findViewById(R.id.discover_info_card_close_button);
    }

    /** Sets the on click listener to the dismiss button of the info card. */
    public void setDismissButtonOnClickListener(View.OnClickListener listener) {
        mDismissButton.setOnClickListener(listener);
    }

    /** Sets the on click listener to open the bottom sheet to part of the description. */
    public void setDescriptionLink(View.OnClickListener listener) {
        String text =
                mContext.getResources()
                        .getString(R.string.supervised_user_discover_info_card_description);
        SpanApplier.SpanInfo spanInfo =
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new TextAppearanceSpan(mContext, R.style.TextAppearance_TextSmall_Link));
        SpannableString formattedText = SpanApplier.applySpans(text, spanInfo);
        mDescription.setText(formattedText);
        mDescription.setOnClickListener(listener);
    }

    public void setCardBackgroundResource() {
        if (ChromeFeatureList.sSurfacePolish.isEnabled()) {
            ColorStateList backgroundTint =
                    ColorStateList.valueOf(
                            ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0));
            GradientDrawable drawable =
                    (GradientDrawable) mMaterialCardViewNoShadow.getBackground();
            drawable.setColor(backgroundTint.getDefaultColor());
        }
    }
}

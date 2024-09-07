// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.ui.ElidedUrlTextView;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.widget.ChromeImageButton;

/** Represents the url, a sub page header and container for page info content. */
public class PageInfoContainer extends FrameLayout {
    public static final float sScale = 0.92f;
    public static final int sOutDuration = 90;
    public static final int sInDuration = 210;

    /**  Parameters to configure the view of page info subpage. */
    public static class Params {
        // The URL to be shown at the top of the page.
        public CharSequence url;
        // The length of the URL's origin in number of characters.
        public int urlOriginLength;
        // The URL to show in truncated state.
        public String truncatedUrl;
        // Whether the close button is displayed.
        public boolean showCloseButton;

        public Runnable urlTitleClickCallback;
        public Runnable urlTitleLongClickCallback;
        public Runnable backButtonClickCallback;
        public Runnable closeButtonClickCallback;
    }

    private ElidedUrlTextView mExpandedUrlTitle;
    private TextView mTruncatedUrlTitle;

    private final ViewGroup mWrapper;
    private final ViewGroup mContent;
    private View mCurrentView;

    private final View mSubpageHeader;
    private final TextView mSubpageTitle;

    public PageInfoContainer(Context context) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info_container, this, true);
        mWrapper = findViewById(R.id.page_info_wrapper);
        mContent = findViewById(R.id.page_info_content);
        mSubpageHeader = findViewById(R.id.page_info_subpage_header);
        mSubpageTitle = findViewById(R.id.page_info_subpage_title);
    }

    public void setParams(Params params) {
        View urlWrapper = findViewById(R.id.page_info_url_wrapper);
        initializeUrlView(urlWrapper, params);

        mExpandedUrlTitle = findViewById(R.id.page_info_url);
        mExpandedUrlTitle.setUrl(params.url, params.urlOriginLength);
        // Adjust mExpandedUrlTitle for displaying the non-truncated URL.
        mExpandedUrlTitle.toggleTruncation();

        // Use a separate view for truncated URL display.
        mTruncatedUrlTitle = findViewById(R.id.page_info_truncated_url);
        mTruncatedUrlTitle = findViewById(R.id.page_info_truncated_url);
        mTruncatedUrlTitle.setText(params.truncatedUrl);

        ChromeImageButton closeButton = findViewById(R.id.page_info_close);
        closeButton.setVisibility(params.showCloseButton ? VISIBLE : GONE);
        closeButton.setOnClickListener(v -> params.closeButtonClickCallback.run());
        View closeButtonLeftPadding = findViewById(R.id.page_info_close_left_padding);
        closeButtonLeftPadding.setVisibility(params.showCloseButton ? VISIBLE : GONE);

        ChromeImageButton backButton = findViewById(R.id.subpage_back_button);
        backButton.setOnClickListener(v -> params.backButtonClickCallback.run());
    }

    private void initializeUrlView(View view, Params params) {
        if (params.urlTitleClickCallback != null) {
            view.setOnClickListener(v -> params.urlTitleClickCallback.run());
        }
        if (params.urlTitleLongClickCallback != null) {
            view.setOnLongClickListener(
                    v -> {
                        params.urlTitleLongClickCallback.run();
                        return true;
                    });
        }
    }

    public void toggleUrlTruncation() {
        boolean showExpanded = mExpandedUrlTitle.getVisibility() != VISIBLE;
        mExpandedUrlTitle.setVisibility(showExpanded ? VISIBLE : GONE);
        mTruncatedUrlTitle.setVisibility(showExpanded ? GONE : VISIBLE);
        announceForAccessibility(
                getResources()
                        .getString(
                                showExpanded
                                        ? R.string.page_info_url_expanded
                                        : R.string.page_info_url_truncated));
    }

    public void setFavicon(Drawable favicon) {
        mTruncatedUrlTitle.setCompoundDrawablesRelative(favicon, null, null, null);
    }

    public void showPage(View view, CharSequence subPageTitle, Runnable onPreviousPageRemoved) {
        if (mCurrentView == null) {
            // Don't animate if there is no current view.
            assert onPreviousPageRemoved == null;
            replaceContentView(view, subPageTitle);
            return;
        }
        // Create "fade-through" animation.
        // TODO(crbug.com/40129299): Animate height change and set correct interpolator.
        mWrapper.animate()
                .setDuration(sOutDuration)
                .alpha(0)
                .setInterpolator(Interpolators.EMPHASIZED_DECELERATE)
                .withEndAction(
                        () -> {
                            replaceContentView(view, subPageTitle);
                            mWrapper.setScaleX(sScale);
                            mWrapper.setScaleY(sScale);
                            mWrapper.setAlpha(0);
                            mWrapper.animate()
                                    .setDuration(sInDuration)
                                    .scaleX(1)
                                    .scaleY(1)
                                    .alpha(1)
                                    .setInterpolator(Interpolators.EMPHASIZED_DECELERATE)
                                    .withEndAction(onPreviousPageRemoved);
                        });
    }

    /** Replaces the current view with |view| and configures the subpage header. */
    private void replaceContentView(View view, CharSequence subPageTitle) {
        mContent.removeAllViews();
        mCurrentView = view;
        mSubpageHeader.setVisibility(subPageTitle != null ? VISIBLE : GONE);
        mSubpageTitle.setText(subPageTitle);
        mContent.addView(view);
        announceForAccessibility(
                subPageTitle != null
                        ? subPageTitle
                        : getResources().getString(R.string.accessibility_toolbar_btn_site_info));
    }
}

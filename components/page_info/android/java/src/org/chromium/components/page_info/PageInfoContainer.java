// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * Represents the url, a sub page header and container for page info content.
 */
public class PageInfoContainer extends FrameLayout {
    /**  Parameters to configure the view of page info subpage. */
    public static class Params {
        // Whether the URL title should be shown.
        public boolean urlTitleShown;
        // The URL to be shown at the top of the page.
        public CharSequence url;
        // The length of the URL's origin in number of characters.
        public int urlOriginLength;
        // The URL to show in truncated state.
        public String truncatedUrl;

        public Runnable urlTitleClickCallback;
        public Runnable urlTitleLongClickCallback;
        public Runnable backButtonClickCallback;
    }
    private PageInfoView.ElidedUrlTextView mUrlTitle;
    private TextView mTruncatedUrlTitle;

    private final View mSubpageHeader;
    private TextView mSubpageTitle;
    private final FrameLayout mContent;

    public PageInfoContainer(Context context) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info_container, this, true);
        mSubpageHeader = findViewById(R.id.page_info_subpage_header);
        mSubpageTitle = findViewById(R.id.page_info_subpage_title);
        mContent = findViewById(R.id.page_info_content);
    }

    public void setParams(Params params) {
        mUrlTitle = findViewById(R.id.page_info_url);
        initializeUrlView(mUrlTitle, params);
        mUrlTitle.setUrl(params.url, params.urlOriginLength);
        // Adjust the mUrlTitle for displaying the non-truncated URL.
        mUrlTitle.toggleTruncation();

        mTruncatedUrlTitle = findViewById(R.id.page_info_truncated_url);
        // Use a separate view for truncated URL display.
        initializeUrlView(mTruncatedUrlTitle, params);
        mTruncatedUrlTitle = findViewById(R.id.page_info_truncated_url);
        mTruncatedUrlTitle.setText(params.truncatedUrl);

        ChromeImageButton backButton = findViewById(R.id.subpage_back_button);
        backButton.setOnClickListener(v -> params.backButtonClickCallback.run());
    }

    private void initializeUrlView(View view, Params params) {
        if (!params.urlTitleShown) {
            view.setVisibility(GONE);
        }
        if (params.urlTitleClickCallback != null) {
            view.setOnClickListener(v -> { params.urlTitleClickCallback.run(); });
        }
        if (params.urlTitleLongClickCallback != null) {
            view.setOnLongClickListener(v -> {
                params.urlTitleLongClickCallback.run();
                return true;
            });
        }
    }

    public void toggleUrlTruncation() {
        mUrlTitle.setVisibility(mTruncatedUrlTitle.getVisibility());
        mTruncatedUrlTitle.setVisibility(mUrlTitle.getVisibility() == VISIBLE ? GONE : VISIBLE);
    }

    public void setFavicon(Drawable favicon) {
        int padding =
                getResources().getDimensionPixelSize(R.dimen.page_info_popup_button_padding_sides);
        int size = getResources().getDimensionPixelSize(R.dimen.page_info_favicon_size);

        favicon.setBounds(0, 0, size, size);
        mTruncatedUrlTitle.setCompoundDrawablePadding(padding);
        mTruncatedUrlTitle.setCompoundDrawablesRelative(favicon, null, null, null);
    }

    public void showPage(View view, CharSequence title, boolean isMainPage) {
        mContent.removeAllViews();
        mContent.addView(view);
        mSubpageHeader.setVisibility(isMainPage ? GONE : VISIBLE);
        mSubpageTitle.setText(title);
    }
}

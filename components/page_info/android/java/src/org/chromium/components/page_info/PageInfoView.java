// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents the view inside the page info popup. */
@NullMarked
public class PageInfoView extends FrameLayout implements OnClickListener {
    private final LinearLayout mRowWrapper;
    private final PageInfoRowView mConnectionRow;
    private final ConnectionSecurityView mConnectionSecurityView;
    private final PageInfoRowView mPermissionsRow;
    private final PageInfoRowView mCookiesRow;
    private final Button mForgetSiteButton;
    private final TextView mHttpsImageCompressionMessage;
    private final Button mOpenOnlineButton;

    /**  Parameters to configure the view of the page info popup. */
    public static class Params {
        public boolean openOnlineButtonShown = true;
        public boolean httpsImageCompressionMessageShown;
        public @Nullable Runnable openOnlineButtonClickCallback;
    }

    public PageInfoView(Context context, Params params) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info, this, true);
        // Elevate the "Cookies and site data" item.
        LinearLayout rowWrapper = (LinearLayout) findViewById(R.id.page_info_row_wrapper);

        mRowWrapper = rowWrapper;
        mCookiesRow = findViewById(R.id.page_info_cookies_row);
        initializePageInfoViewChild(rowWrapper, true, null);
        mConnectionRow = findViewById(R.id.page_info_connection_row);
        mConnectionSecurityView = findViewById(R.id.page_info_connection_security);
        mPermissionsRow = findViewById(R.id.page_info_permissions_row);
        mForgetSiteButton = findViewById(R.id.page_info_forget_site_button);
        initializePageInfoViewChild(mForgetSiteButton, false, null);
        mHttpsImageCompressionMessage =
                findViewById(R.id.page_info_lite_mode_https_image_compression_message);
        initializePageInfoViewChild(
                mHttpsImageCompressionMessage, params.httpsImageCompressionMessageShown, null);
        mOpenOnlineButton = findViewById(R.id.page_info_open_online_button);
        // The open online button should not fade in.
        initializePageInfoViewChild(
                mOpenOnlineButton,
                params.openOnlineButtonShown,
                params.openOnlineButtonClickCallback);
    }

    public PageInfoRowView getConnectionRowView() {
        return mConnectionRow;
    }

    public ConnectionSecurityView getConnectionSecurityView() {
        return mConnectionSecurityView;
    }

    public PageInfoRowView getPermissionsRowView() {
        return mPermissionsRow;
    }

    public PageInfoRowView getCookiesRowView() {
        return mCookiesRow;
    }

    public ViewGroup getRowWrapper() {
        return mRowWrapper;
    }

    public Button getForgetSiteButton() {
        return mForgetSiteButton;
    }

    private void initializePageInfoViewChild(
            View child, boolean shown, @Nullable Runnable clickCallback) {
        child.setVisibility(shown ? View.VISIBLE : View.GONE);
        child.setTag(R.id.page_info_click_callback, clickCallback);
        if (clickCallback == null) return;
        child.setOnClickListener(this);
    }

    // FrameLayout override.
    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
    }

    // OnClickListener interface.
    @Override
    public void onClick(View view) {
        Object clickCallbackObj = view.getTag(R.id.page_info_click_callback);
        if (!(clickCallbackObj instanceof Runnable)) {
            throw new IllegalStateException("Unable to find click callback for view: " + view);
        }
        Runnable clickCallback = (Runnable) clickCallbackObj;
        clickCallback.run();
    }
}

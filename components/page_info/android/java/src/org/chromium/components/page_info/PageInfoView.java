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

/** Represents the view inside the page info popup. */
public class PageInfoView extends FrameLayout implements OnClickListener {
    private static final int COOKIES_ROW_POSITION = 1;

    private LinearLayout mRowWrapper;
    private PageInfoRowView mConnectionRow;
    private PageInfoRowView mPermissionsRow;
    private PageInfoRowView mCookiesRow;
    private Button mForgetSiteButton;
    private TextView mHttpsImageCompressionMessage;
    private Button mOpenOnlineButton;

    /**  Parameters to configure the view of the page info popup. */
    public static class Params {
        public boolean openOnlineButtonShown = true;
        public boolean httpsImageCompressionMessageShown;
        public Runnable openOnlineButtonClickCallback;
    }

    public PageInfoView(Context context, Params params) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info, this, true);
        // Elevate the "Cookies and site data" item.
        LinearLayout rowWrapper = (LinearLayout) findViewById(R.id.page_info_row_wrapper);
        PageInfoRowView cookiesRow = (PageInfoRowView) findViewById(R.id.page_info_cookies_row);
        rowWrapper.removeView(cookiesRow);
        rowWrapper.addView(cookiesRow, COOKIES_ROW_POSITION);
        init(params);
    }

    private void init(Params params) {
        initRowWrapper();
        initConnection();
        initPermissions();
        initCookies();
        initForgetSiteButton();
        initHttpsImageCompression(params);
        initOpenOnline(params);
    }

    private void initRowWrapper() {
        mRowWrapper = findViewById(R.id.page_info_row_wrapper);
        initializePageInfoViewChild(mRowWrapper, true, null);
    }

    private void initConnection() {
        mConnectionRow = findViewById(R.id.page_info_connection_row);
    }

    private void initPermissions() {
        mPermissionsRow = findViewById(R.id.page_info_permissions_row);
    }

    private void initCookies() {
        mCookiesRow = findViewById(R.id.page_info_cookies_row);
    }

    private void initForgetSiteButton() {
        mForgetSiteButton = findViewById(R.id.page_info_forget_site_button);
        initializePageInfoViewChild(mForgetSiteButton, false, null);
    }

    private void initHttpsImageCompression(Params params) {
        mHttpsImageCompressionMessage =
                findViewById(R.id.page_info_lite_mode_https_image_compression_message);
        initializePageInfoViewChild(
                mHttpsImageCompressionMessage, params.httpsImageCompressionMessageShown, null);
    }

    private void initOpenOnline(Params params) {
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

    private void initializePageInfoViewChild(View child, boolean shown, Runnable clickCallback) {
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

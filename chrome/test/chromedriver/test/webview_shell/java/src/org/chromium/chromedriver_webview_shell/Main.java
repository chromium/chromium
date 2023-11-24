// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromedriver_webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Window;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

public class Main extends Activity {
    private WebView mWebView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().requestFeature(Window.FEATURE_PROGRESS);
        setContentView(R.layout.main_layout);

        WebView.setWebContentsDebuggingEnabled(true);
        mWebView = (WebView) findViewById(R.id.webview);
        WebSettings webSettings = mWebView.getSettings();
        webSettings.setJavaScriptEnabled(true);

        final Activity activity = this;
        mWebView.setWebChromeClient(
                new WebChromeClient() {
                    @Override
                    public void onProgressChanged(WebView view, int progress) {
                        activity.setProgress(progress * 100);
                    }
                });
        mWebView.setWebViewClient(
                new WebViewClient() {
                    @Override
                    public void onReceivedError(
                            WebView view, int errorCode, String description, String failingUrl) {
                        Toast.makeText(activity, "Error: " + description, Toast.LENGTH_SHORT)
                                .show();
                    }
                });

        loadUrl(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        loadUrl(intent);
    }

    private void loadUrl(Intent intent) {
        if (intent != null && intent.getDataString() != null) {
            mWebView.loadUrl(intent.getDataString());
        }
    }
}

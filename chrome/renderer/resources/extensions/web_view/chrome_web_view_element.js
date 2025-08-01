// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module registers the chrome-specific <webview> Element.

const ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
const forwardApiMethods =
    require('guestViewContainerElement').forwardApiMethods;
const registerElement = require('guestViewContainerElement').registerElement;
const WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
const WebViewElement = require('webViewElement').WebViewElement;
const WebViewInternal = getInternalApi('webViewInternal');
const WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;

class ChromeWebViewElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ChromeWebViewImpl(this);
    privates(this).originalGo = originalGo;
  }
}

// Forward remaining ChromeWebViewElement.foo* method calls to
// ChromeWebViewImpl.foo* or WebViewInternal.foo*. WebView APIs don't support
// promise-based syntax so |promiseMethodDetails| is left empty.
forwardApiMethods(
    ChromeWebViewElement, ChromeWebViewImpl, WebViewInternal,
    WEB_VIEW_API_METHODS, /*promiseMethodDetails=*/[]);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// |ChromeWebViewElement.prototype.go|.
const originalGo = ChromeWebViewElement.prototype.go;

registerElement('WebView', 'WebView', ChromeWebViewElement);

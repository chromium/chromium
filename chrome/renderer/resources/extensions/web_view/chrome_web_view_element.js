// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module registers the chrome-specific <webview> Element.

var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var registerElement = require('guestViewContainerElement').registerElement;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewInternal = getInternalApi('webViewInternal');
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;

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
var originalGo = ChromeWebViewElement.prototype.go;

registerElement('WebView', ChromeWebViewElement);

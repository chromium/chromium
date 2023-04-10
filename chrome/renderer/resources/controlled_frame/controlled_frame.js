// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <controlledframe> Element.

var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var registerElement = require('guestViewContainerElement').registerElement;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;

class ControlledFrameElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ChromeWebViewImpl(this);
  }
}

registerElement('ControlledFrame', ControlledFrameElement);

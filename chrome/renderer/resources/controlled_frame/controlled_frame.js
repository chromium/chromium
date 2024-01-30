// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <controlledframe> Element.

var ControlledFrameImpl = require('controlledFrameImpl').ControlledFrameImpl;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var CONTROLLED_FRAME_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_API_METHODS;
var CONTROLLED_FRAME_PROMISE_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_PROMISE_API_METHODS;
var registerElement = require('guestViewContainerElement').registerElement;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewInternal = getInternalApi('webViewInternal');

class ControlledFrameElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ControlledFrameImpl(this);
    privates(this).originalGo = originalGo;
  }
}

// Forward remaining ControlledFrameElement.foo* method calls to
// ChromeWebViewImpl.foo* or WebViewInternal.foo*.
forwardApiMethods(
    ControlledFrameElement, ControlledFrameImpl, WebViewInternal,
    CONTROLLED_FRAME_API_METHODS, CONTROLLED_FRAME_PROMISE_API_METHODS);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// ControlledFrameElement.prototype.go|.
var originalGo = ControlledFrameElement.prototype.go;

registerElement('ControlledFrame', ControlledFrameElement);

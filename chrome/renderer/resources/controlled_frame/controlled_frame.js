// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <controlledframe> Element.

var ControlledFrameImpl = require('controlledFrameImpl').ControlledFrameImpl;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var promiseWrap = require('guestViewContainerElement').promiseWrap;
var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var CONTROLLED_FRAME_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_API_METHODS;
var CONTROLLED_FRAME_DELETED_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_DELETED_API_METHODS;
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

  // Override add/removeContentScripts to accept a `callback` parameter
  // so they can be used with Promises.
  addContentScripts(rules, callback) {
    var internal = privates(this).internal;
    return WebViewInternal.addContentScripts(
        internal.viewInstanceId, rules, callback);
  }

  removeContentScripts(names, callback) {
    var internal = privates(this).internal;
    return WebViewInternal.removeContentScripts(
        internal.viewInstanceId, names, callback);
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

// Wrap callback methods in promise handlers. Note: This disables the callback
// forms.
promiseWrap(ControlledFrameElement, ControlledFrameImpl, WebViewInternal,
            CONTROLLED_FRAME_PROMISE_API_METHODS);

// Delete GuestView methods that should not be part of the Controlled Frame API.
(function() {
  for (const methodName of CONTROLLED_FRAME_DELETED_API_METHODS) {
    let clazz = ControlledFrameElement.prototype;
    while ((methodName in clazz) && clazz.constructor.name !== 'HTMLElement') {
      delete clazz[methodName];
      clazz = $Object.getPrototypeOf(clazz);
    }
  }
})();

registerElement('ControlledFrame', ControlledFrameElement);

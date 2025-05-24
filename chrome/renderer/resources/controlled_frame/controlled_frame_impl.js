// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CHROME_WEB_VIEW_CONTEXT_MENUS_PROMISE_API_METHODS =
    require('chromeWebViewContextMenusApiMethods').PROMISE_API_METHODS;
var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var WebViewContextMenusImpl = require('chromeWebView').WebViewContextMenusImpl;
var ControlledFrameInternal = getInternalApi('controlledFrameInternal');
var ControlledFrameEvents =
    require('controlledFrameEvents').ControlledFrameEvents;
var logging = requireNative('logging');
var promiseWrap = require('guestViewContainerElement').promiseWrap;
var utils = require('utils');

function ControlledFrameContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
}
$Object.setPrototypeOf(ControlledFrameContextMenusImpl.prototype,
  WebViewContextMenusImpl.prototype);

function getCallbackIndex(name) {
  var foundMethodDetails = undefined;
  for (const methodDetails of
        CHROME_WEB_VIEW_CONTEXT_MENUS_PROMISE_API_METHODS) {
    if (methodDetails.name === name) {
      foundMethodDetails = methodDetails;
      break;
    }
  }
  logging.CHECK(
      foundMethodDetails !== undefined,
      "could not find context menus method details");
  return foundMethodDetails.callbackIndex;
}

ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased =
    function(handler, name) {
  var callbackIndex = getCallbackIndex(name);
  // TODO(crbug.com/378956568): Verify these methods don't require an instance
  // ID check.
  function verifyEnvironment(reject) { return true; }
  return function(var_args) {
    return promiseWrap(handler.bind(this), arguments, callbackIndex,
                       verifyEnvironment, /*callbackAllowed=*/true);
  };
}

// Controlled Frame has its own internal definition of Context Menus create().
ControlledFrameContextMenusImpl.prototype.createImpl = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
      ControlledFrameInternal.contextMenusCreate, null, args);
}

// Controlled Frame has its own internal definition of Context Menus update().
ControlledFrameContextMenusImpl.prototype.updateImpl = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
    ControlledFrameInternal.contextMenusUpdate, null, args);
}

ControlledFrameContextMenusImpl.prototype.create =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        ControlledFrameContextMenusImpl.prototype.createImpl, "create");

ControlledFrameContextMenusImpl.prototype.remove =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        WebViewContextMenusImpl.prototype.remove, "remove");

ControlledFrameContextMenusImpl.prototype.removeAll =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        WebViewContextMenusImpl.prototype.removeAll,
        "removeAll");

ControlledFrameContextMenusImpl.prototype.update =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
      ControlledFrameContextMenusImpl.prototype.updateImpl, "update");

function ControlledFrameContextMenus() {
  privates(ControlledFrameContextMenus).constructPrivate(this, arguments);
}

utils.expose(ControlledFrameContextMenus, ControlledFrameContextMenusImpl, {
  functions: [
    'create',
    'remove',
    'removeAll',
    'update',
  ]
});

class ControlledFrameImpl extends ChromeWebViewImpl {
  constructor(webviewElement) {
    super(webviewElement);
  }

  setupEvents() {
    new ControlledFrameEvents(this);
  }

  createWebViewContextMenus() {
    return new ControlledFrameContextMenus(this.viewInstanceId);
  }

  setClientHintsUABrandEnabled(enable) {
    ControlledFrameInternal.setClientHintsEnabled(this.guest.getId(), !!enable);
  }

  getLogTag() {
    return 'controlledframe';
  }
}

exports.$set('ControlledFrameImpl', ControlledFrameImpl);

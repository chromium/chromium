// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
var WebViewContextMenusImpl = require('chromeWebView').WebViewContextMenusImpl;
var ControlledFrame = getInternalApi('controlledFrameInternal');
var ControlledFrameEvents =
    require('controlledFrameEvents').ControlledFrameEvents;
var utils = require('utils');

function ControlledFrameContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
}
$Object.setPrototypeOf(ControlledFrameContextMenusImpl.prototype,
  WebViewContextMenusImpl.prototype);

ControlledFrameContextMenusImpl.prototype.create = function() {
  let args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  let result = $Function.apply(ControlledFrame.contextMenusCreate, null, args);
  if (bindingUtil.hasLastError()) {
    result = bindingUtil.getLastErrorMessage();
    bindingUtil.clearLastError();
  }
  return result;
}

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
}

exports.$set('ControlledFrameImpl', ControlledFrameImpl);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
const ControlledFrameInternal = getInternalApi('controlledFrameInternal');
const ControlledFrameEvents =
    require('controlledFrameEvents').ControlledFrameEvents;
const ControlledFrameContextMenus =
    require('controlledFrameContextMenus').ControlledFrameContextMenus;

class ControlledFrameImpl extends ChromeWebViewImpl {
  constructor(webviewElement) {
    super(webviewElement);
  }

  setupEvents() {
    new ControlledFrameEvents(this);
  }

  createWebViewContextMenus() {
    return new ControlledFrameContextMenus(this, this.viewInstanceId);
  }

  setClientHintsUABrandEnabled(enable) {
    ControlledFrameInternal.setClientHintsEnabled(this.guest.getId(), !!enable);
  }

  getLogTag() {
    return 'controlledframe';
  }
}

exports.$set('ControlledFrameImpl', ControlledFrameImpl);

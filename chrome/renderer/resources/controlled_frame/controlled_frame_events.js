// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const WebViewEvents = require('webViewEvents').WebViewEvents;

class ControlledFrameEvents extends WebViewEvents {
  getEvents() {
    const webViewEvents = super.getEvents();
    delete webViewEvents['findupdate'];
    return webViewEvents;
  }
}

// Exports.
exports.$set('ControlledFrameEvents', ControlledFrameEvents);

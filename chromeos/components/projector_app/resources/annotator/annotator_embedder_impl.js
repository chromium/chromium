// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'annotator-embedder-impl',

  behaviors: [WebUIListenerBehavior],

  properties: {},

  // TODO(b/194915295): Add WebUIListener methods here.

  /** @override */
  ready() {
    // TODO(b/194915295): Register WebUIListener methods here.
  },

});

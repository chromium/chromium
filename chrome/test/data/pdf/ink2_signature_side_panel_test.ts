// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerSignatureSidePanelElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Ink2SignatureSidePanel', function() {
  let sidePanel: ViewerSignatureSidePanelElement;

  setup(function() {
    document.body.innerHTML = '';
    sidePanel = document.createElement('viewer-signature-side-panel');
    document.body.appendChild(sidePanel);
  });

  // Test that the side panel has a signature selector element.
  test('HasSignatureSelector', function() {
    assertTrue(!!sidePanel.shadowRoot.querySelector('signature-selector'));
  });
});

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LensSidePanelPageHandlerInterface} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the LensSidePanelPageHandler used to verify calls to the
 * browser from WebUI.
 */
export class TestLensSidePanelPageHandler extends TestBrowserProxy implements
    LensSidePanelPageHandlerInterface {
  constructor() {
    super([
      'popAndLoadQueryFromHistory',
      'getIsContextualSearchbox',
      'onScrollToMessage',
      'requestSendFeedback',
      'onAimMessage',
      'onImageQueryWithEmptyText',
    ]);
  }

  popAndLoadQueryFromHistory() {
    this.methodCalled('popAndLoadQueryFromHistory');
  }

  getIsContextualSearchbox(): Promise<{isContextualSearchbox: boolean}> {
    this.methodCalled('getIsContextualSearchbox');
    return Promise.resolve({isContextualSearchbox: false});
  }

  onScrollToMessage(textFragments: string[], pdfPageNumber: number) {
    this.methodCalled(
        'onScrollToMessage',
        textFragments,
        pdfPageNumber,
    );
  }

  onAimMessage(message: number[]) {
    this.methodCalled('onAimMessage', message);
  }

  requestSendFeedback() {
    this.methodCalled('requestSendFeedback');
  }

  onImageQueryWithEmptyText() {
    this.methodCalled('onImageQueryWithEmptyText');
  }
}

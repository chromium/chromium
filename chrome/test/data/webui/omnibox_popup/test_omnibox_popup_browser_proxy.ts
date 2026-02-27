// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OmniboxPopupPageCallbackRouter, OmniboxPopupPageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestOmniboxPopupBrowserProxy {
  callbackRouter: OmniboxPopupPageCallbackRouter;
  handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  page: OmniboxPopupPageRemote;

  constructor() {
    this.callbackRouter = new OmniboxPopupPageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
  }
}

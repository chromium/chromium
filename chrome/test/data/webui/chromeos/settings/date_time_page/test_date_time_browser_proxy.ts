// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DateTimeBrowserProxy, DateTimePageCallbackRouter, DateTimePageHandlerRemote, DateTimePageRemote} from 'chrome://os-settings/os_settings.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export interface ProxyOptions {
  fakeTimezones?: string[][];
}

/**
 * A fake BrowserProxy implementation that enables switching out the real one to
 * mock various mojo responses.
 */
export class TestDateTimeBrowserProxy implements DateTimeBrowserProxy {
  handler: TestMock<DateTimePageHandlerRemote>&DateTimePageHandlerRemote;
  observer: DateTimePageCallbackRouter;
  observerRemote: DateTimePageRemote;

  constructor(options: ProxyOptions) {
    this.handler = TestMock.fromClass(DateTimePageHandlerRemote);
    this.observer = new DateTimePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();

    this.handler.setResultFor(
        'getTimezones', Promise.resolve({timezones: options.fakeTimezones}));
  }
}

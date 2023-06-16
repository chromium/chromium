// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {TimeZoneBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTimeZoneBrowserProxy extends TestBrowserProxy implements
    TimeZoneBrowserProxy {
  private fakeTimeZones_: string[][];

  constructor() {
    super([
      'dateTimePageReady',
      'getTimeZones',
      'showParentAccessForTimeZone',
      'showSetDateTimeUi',
    ]);
    this.fakeTimeZones_ = [];
  }

  dateTimePageReady(): void {
    this.methodCalled('dateTimePageReady');
  }

  getTimeZones(): Promise<string[][]> {
    this.methodCalled('getTimeZones');
    return Promise.resolve(this.fakeTimeZones_);
  }

  setTimeZones(timeZones: string[][]): void {
    this.fakeTimeZones_ = timeZones;
  }

  showParentAccessForTimeZone(): void {
    this.methodCalled('showParentAccessForTimeZone');
  }

  showSetDateTimeUi(): void {
    this.methodCalled('showSetDateTimeUi');
  }
}

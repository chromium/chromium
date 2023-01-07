// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Account} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export function setTestArcAccountPickerBrowserProxy(
    testBrowserProxy: TestArcAccountPickerBrowserProxy): void;
export function getFakeAccountsNotAvailableInArcList(): Account[];

export class TestArcAccountPickerBrowserProxy extends TestBrowserProxy {
  setAccountsNotAvailableInArc(accountsNotAvailableInArc: Account[]): void;
}

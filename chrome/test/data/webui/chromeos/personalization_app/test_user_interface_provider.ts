// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserInfo, UserProviderInterface} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserProvider extends TestBrowserProxy implements
    UserProviderInterface {
  public info: UserInfo = {name: 'test name', email: 'test@email'};

  constructor() {
    super(['getUserInfo']);
  }

  async getUserInfo(): Promise<{userInfo: UserInfo}> {
    this.methodCalled('getUserInfo');
    return Promise.resolve({userInfo: this.info});
  }
}

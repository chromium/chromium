// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DefaultUserImage, UserInfo, UserProviderInterface} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserProvider extends TestBrowserProxy implements
    UserProviderInterface {
  public info: UserInfo = {
    avatar: {url: 'data://avatar-url'},
    name: 'test name',
    email: 'test@email',
  };

  public defaultUserImages: Array<DefaultUserImage> = [
    {
      index: 8,
      title: {data: 'Test title'.split('').map(ch => ch.charCodeAt(0))},
      url: {url: 'data://test_url'},
    },
  ];

  constructor() {
    super([
      'getDefaultUserImages',
      'getUserInfo',
    ]);
  }

  async getUserInfo(): Promise<{userInfo: UserInfo}> {
    this.methodCalled('getUserInfo');
    return Promise.resolve({userInfo: this.info});
  }

  async getDefaultUserImages():
      Promise<{defaultUserImages: Array<DefaultUserImage>}> {
    this.methodCalled('getDefaultUserImages');
    return Promise.resolve({defaultUserImages: this.defaultUserImages});
  }
}

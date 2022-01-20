// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DefaultUserImage, UserImageObserverRemote, UserInfo, UserProviderInterface} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserProvider extends TestBrowserProxy implements
    UserProviderInterface {
  public defaultUserImages: Array<DefaultUserImage> = [
    {
      index: 8,
      title: {data: 'Test title'.split('').map(ch => ch.charCodeAt(0))},
      url: {url: 'data://test_url'},
    },
  ];

  public image: Url = {url: 'data://avatar-url'};

  public info: UserInfo = {
    name: 'test name',
    email: 'test@email',
  };

  constructor() {
    super([
      'setUserImageObserver',
      'getDefaultUserImages',
      'getUserInfo',
      'selectDefaultImage',
    ]);
  }

  setUserImageObserver(observer: UserImageObserverRemote) {
    this.methodCalled('setUserImageObserver', observer);
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

  selectDefaultImage(index: number) {
    this.methodCalled('selectDefaultImage', index);
  }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DefaultUserImage, UserImage, UserImageObserverRemote, UserInfo, UserProviderInterface} from 'chrome://personalization/js/personalization_app.js';
import {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserProvider extends TestBrowserProxy implements
    UserProviderInterface {
  defaultUserImages: DefaultUserImage[] = [
    {
      index: 8,
      title: {data: 'Test title'.split('').map(ch => ch.charCodeAt(0))},
      url: {url: 'data://test_url'},
      sourceInfo: null,
    },
  ];

  image: UserImage = {defaultImage: this.defaultUserImages[0]} as any;

  info: UserInfo = {
    name: 'test name',
    email: 'test@email',
  };

  profileImage: Url = {
    url: 'data://test_profile_url',
  };

  constructor() {
    super([
      'setUserImageObserver',
      'getDefaultUserImages',
      'selectProfileImage',
      'getUserInfo',
      'selectDefaultImage',
      'selectCameraImage',
      'selectImageFromDisk',
      'selectLastExternalUserImage',
    ]);
  }

  userImageObserverRemote: UserImageObserverRemote|null = null;

  setUserImageObserver(remote: UserImageObserverRemote) {
    this.methodCalled('setUserImageObserver');
    this.userImageObserverRemote = remote;
  }

  async getUserInfo(): Promise<{userInfo: UserInfo}> {
    this.methodCalled('getUserInfo');
    return Promise.resolve({userInfo: this.info});
  }

  async getDefaultUserImages():
      Promise<{defaultUserImages: DefaultUserImage[]}> {
    this.methodCalled('getDefaultUserImages');
    return Promise.resolve({defaultUserImages: this.defaultUserImages});
  }

  selectDefaultImage(index: number) {
    this.methodCalled('selectDefaultImage', index);
  }

  async selectProfileImage() {
    this.methodCalled('selectProfileImage');
    this.profileImage = {
      url: 'data://updated_test_url',
    };
  }

  selectCameraImage(data: BigBuffer) {
    this.methodCalled('selectCameraImage', data);
  }

  selectImageFromDisk() {
    this.methodCalled('selectImageFromDisk');
  }

  selectLastExternalUserImage() {
    this.methodCalled('selectLastExternalUserImage');
  }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenProviderInterface, SeaPenThumbnail} from 'chrome://personalization/js/personalization_app.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSeaPenProvider extends TestBrowserProxy implements
    SeaPenProviderInterface {
  images: SeaPenThumbnail[] = [
    {
      id: 1,
      image: {url: 'https://sea-pen-images.googleusercontent.com/1'},
    },
    {
      id: 2,
      image: {url: 'https://sea-pen-images.googleusercontent.com/2'},
    },
    {
      id: 3,
      image: {url: 'https://sea-pen-images.googleusercontent.com/3'},
    },
    {
      id: 4,
      image: {url: 'https://sea-pen-images.googleusercontent.com/4'},
    },
  ];

  constructor() {
    super([
      'searchWallpaper',
    ]);
  }

  searchWallpaper(text: string) {
    this.methodCalled('searchWallpaper', text);
    return Promise.resolve({images: this.images});
  }
}

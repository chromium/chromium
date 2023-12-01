// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail, SeaPenWallpaper} from 'chrome://personalization/js/personalization_app.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
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

  seaPenWallpapers: SeaPenWallpaper[] = [
    {
      query_info: 'a close up of a flower with water drops on it',
      url: {url: 'https://images.googleusercontent.com/image_1.jpg'},
      file_path: {path: '/sea_pen/image_1.jpg'},
    },
    {
      query_info:
          'a large white ball in the middle of a field with soap bubbles',
      url: {url: 'https://images.googleusercontent.com/image_2.jpg'},
      file_path: {'path': '/sea_pen/image_2.jpg'},
    },
    {
      query_info: 'a large rock sitting on top of a hill in the desert',
      url: {url: 'https://images.googleusercontent.com/image_3.jpg'},
      file_path: {'path': '/sea_pen/image_3.jpg'},
    },
  ];

  constructor() {
    super([
      'searchWallpaper',
      'selectSeaPenThumbnail',
      'selectRecentSeaPenImage',
    ]);
  }

  searchWallpaper(query: SeaPenQuery) {
    this.methodCalled('searchWallpaper', query);
    return Promise.resolve({images: this.images});
  }

  selectSeaPenThumbnail(id: number) {
    this.methodCalled('selectSeaPenThumbnail', id);
    return Promise.resolve({success: true});
  }

  selectRecentSeaPenImage(filePath: FilePath) {
    this.methodCalled('selectRecentSeaPenImage', filePath);
    return Promise.resolve({success: true});
  }
}

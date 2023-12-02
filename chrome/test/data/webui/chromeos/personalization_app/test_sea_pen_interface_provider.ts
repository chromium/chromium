// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RecentSeaPenData, SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from 'chrome://personalization/js/personalization_app.js';
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

  recentImages: FilePath[] = [
    {path: '/sea_pen/111.jpg'},
    {path: '/sea_pen/222.jpg'},
    {path: '/sea_pen/333.jpg'},
  ];

  recentImageData: Record<string, RecentSeaPenData> = {
    '/sea_pen/111.jpg': {
      url: {url: 'data:image/jpeg;base64,image111data'},
      queryInfo: 'query 1',
    },
    '/sea_pen/222.jpg': {
      url: {url: 'data:image/jpeg;base64,image222data'},
      queryInfo: 'query 2',
    },
    '/sea_pen/333.jpg': {
      url: {url: 'data:image/jpeg;base64,image333data'},
      queryInfo: 'query 3',
    },
  };

  constructor() {
    super([
      'searchWallpaper',
      'selectSeaPenThumbnail',
      'selectRecentSeaPenImage',
      'getRecentSeaPenImages',
      'getRecentSeaPenImageThumbnail',
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

  getRecentSeaPenImages() {
    this.methodCalled('getRecentSeaPenImages');
    return Promise.resolve({images: this.recentImages});
  }

  getRecentSeaPenImageThumbnail(filePath: FilePath) {
    this.methodCalled('getRecentSeaPenImageThumbnail', filePath);
    return Promise.resolve({url: this.recentImageData[filePath.path]!.url});
  }
}

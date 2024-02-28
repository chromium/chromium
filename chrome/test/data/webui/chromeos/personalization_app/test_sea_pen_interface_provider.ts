// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RecentSeaPenData} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenImageId} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {MantaStatusCode, SeaPenFeedbackMetadata, SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {isSeaPenImageId} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  recentImageIds: SeaPenImageId[] = [
    111,
    222,
    333,
  ];

  recentImageData: Record<SeaPenImageId, RecentSeaPenData> = {
    111: {
      url: {url: 'data:image/jpeg;base64,image111data'},
      queryInfo: 'query 1',
    },
    222: {
      url: {url: 'data:image/jpeg;base64,image222data'},
      queryInfo: 'query 2',
    },
    333: {
      url: {url: 'data:image/jpeg;base64,image333data'},
      queryInfo: 'query 3',
    },
  };

  selectSeaPenThumbnailResponse:
      ReturnType<SeaPenProviderInterface['selectSeaPenThumbnail']> =
          Promise.resolve({success: true});

  shouldShowSeaPenTermsOfServiceDialogResponse = true;

  constructor() {
    super([
      'searchWallpaper',
      'selectSeaPenThumbnail',
      'selectRecentSeaPenImage',
      'getRecentSeaPenImages',
      'getRecentSeaPenImageThumbnail',
      'deleteRecentSeaPenImage',
      'shouldShowSeaPenTermsOfServiceDialog',
      'handleSeaPenTermsOfServiceAccepted',
    ]);
  }

  searchWallpaper(query: SeaPenQuery) {
    this.methodCalled('searchWallpaper', query);
    return Promise.resolve({
      images: this.images,
      statusCode: MantaStatusCode.kOk,
    });
  }

  selectSeaPenThumbnail(id: SeaPenImageId) {
    assertTrue(
        isSeaPenImageId(id), `id must be SeaPenImageId but received: ${id}`);
    this.methodCalled('selectSeaPenThumbnail', id);
    return this.selectSeaPenThumbnailResponse;
  }

  selectRecentSeaPenImage(id: SeaPenImageId) {
    assertTrue(
        isSeaPenImageId(id), `id must be SeaPenImageId but received: ${id}`);
    this.methodCalled('selectRecentSeaPenImage', id);
    return Promise.resolve({success: true});
  }

  getRecentSeaPenImages() {
    this.methodCalled('getRecentSeaPenImages');
    return Promise.resolve({ids: this.recentImageIds});
  }

  getRecentSeaPenImageThumbnail(id: SeaPenImageId) {
    assertTrue(
        isSeaPenImageId(id), `id must be SeaPenImageId but received: ${id}`);
    this.methodCalled('getRecentSeaPenImageThumbnail', id);
    return Promise.resolve({url: this.recentImageData[id]!.url});
  }

  deleteRecentSeaPenImage(id: SeaPenImageId) {
    assertTrue(
        isSeaPenImageId(id), `id must be SeaPenImageId but received: ${id}`);
    this.methodCalled('deleteRecentSeaPenImage', id);
    this.recentImageIds = this.recentImageIds.filter(x => x !== id);
    return Promise.resolve({success: true});
  }

  openFeedbackDialog(metadata: SeaPenFeedbackMetadata): void {
    this.methodCalled('openFeedbackDialog', metadata);
    return;
  }

  shouldShowSeaPenTermsOfServiceDialog() {
    this.methodCalled('shouldShowSeaPenTermsOfServiceDialog');
    return Promise.resolve(
        {shouldShowDialog: this.shouldShowSeaPenTermsOfServiceDialogResponse});
  }

  handleSeaPenTermsOfServiceAccepted() {
    this.methodCalled('handleSeaPenTermsOfServiceAccepted');
    this.shouldShowSeaPenTermsOfServiceDialogResponse = false;
  }
}

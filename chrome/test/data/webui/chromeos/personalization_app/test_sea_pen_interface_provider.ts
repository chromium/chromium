// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenImageId} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {MantaStatusCode, RecentSeaPenImageInfo, RecentSeaPenThumbnailData, SeaPenFeedbackMetadata, SeaPenObserverInterface, SeaPenObserverRemote, SeaPenProviderInterface, SeaPenQuery, SeaPenThumbnail} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {isSeaPenImageId} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSeaPenProvider extends TestBrowserProxy implements
    SeaPenProviderInterface {
  seaPenObserverRemote: SeaPenObserverInterface|null = null;

  thumbnails: SeaPenThumbnail[] = [
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

  seaPenQuery: SeaPenQuery = {
    templateQuery: {
      id: SeaPenTemplateId.kGlowscapes,
      options: Object.fromEntries([
        [
          SeaPenTemplateChip.kGlowscapesLandscape,
          SeaPenTemplateOption.kGlowscapesLandscapeCoralReef,
        ],
        [
          SeaPenTemplateChip.kGlowscapesFeature,
          SeaPenTemplateOption.kGlowscapesFeatureFlower,
        ],
      ]),
      userVisibleQuery: {
        text: 'test template query',
        templateTitle: 'test template title',
      },
    },
  };

  seaPenFreeformQuery: SeaPenQuery = {
    textQuery: 'test freeform query',
  };

  recentImageInfo2: RecentSeaPenImageInfo = {
    query: this.seaPenQuery,
    creationTime: stringToMojoString16('Dec 15, 2023'),
  };

  recentImageInfo3: RecentSeaPenImageInfo = {
    query: {
      textQuery: 'test freeform query',
    },
    creationTime: stringToMojoString16('Dec 31, 2023'),
  };

  recentImageData: Record<string, RecentSeaPenThumbnailData|null> = {
    111: {
      url: {url: 'data:image/jpeg;base64,image111data'},
      imageInfo: null,
    },
    222: {
      url: {url: 'data:image/jpeg;base64,image222data'},
      imageInfo: this.recentImageInfo2,
    },
    333: {
      url: {url: 'data:image/jpeg;base64,image333data'},
      imageInfo: this.recentImageInfo3,
    },
  };

  selectSeaPenThumbnailResponse:
      ReturnType<SeaPenProviderInterface['selectSeaPenThumbnail']> =
          Promise.resolve({success: true});

  selectSeaPenRecentImageResponse:
      ReturnType<SeaPenProviderInterface['selectRecentSeaPenImage']> =
          Promise.resolve({success: true});

  shouldShowSeaPenIntroductionDialogResponse = true;

  isInTabletModeResponse = false;

  constructor() {
    super([
      'setSeaPenObserver',
      'getSeaPenThumbnails',
      'selectSeaPenThumbnail',
      'selectRecentSeaPenImage',
      'getRecentSeaPenImageIds',
      'getRecentSeaPenImageThumbnail',
      'deleteRecentSeaPenImage',
      'shouldShowSeaPenIntroductionDialog',
      'handleSeaPenIntroductionDialogClosed',
      'isInTabletMode',
      'makeTransparent',
    ]);
  }

  setSeaPenObserver(observer: SeaPenObserverRemote) {
    this.methodCalled('setSeaPenObserver', observer);
    this.seaPenObserverRemote = observer;
  }

  getSeaPenThumbnails(query: SeaPenQuery) {
    this.methodCalled('getSeaPenThumbnails', query);
    return Promise.resolve({
      thumbnails: this.thumbnails,
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
    return this.selectSeaPenRecentImageResponse;
  }

  getRecentSeaPenImageIds() {
    this.methodCalled('getRecentSeaPenImageIds');
    return Promise.resolve({ids: this.recentImageIds});
  }

  getRecentSeaPenImageThumbnail(id: SeaPenImageId) {
    assertTrue(
        isSeaPenImageId(id), `id must be SeaPenImageId but received: ${id}`);
    this.methodCalled('getRecentSeaPenImageThumbnail', id);
    return Promise.resolve({
      thumbnailData: this.recentImageData[id]!,
    });
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

  shouldShowSeaPenIntroductionDialog() {
    this.methodCalled('shouldShowSeaPenIntroductionDialog');
    return Promise.resolve(
        {shouldShowDialog: this.shouldShowSeaPenIntroductionDialogResponse});
  }

  handleSeaPenIntroductionDialogClosed() {
    this.methodCalled('handleSeaPenIntroductionDialogClosed');
    this.shouldShowSeaPenIntroductionDialogResponse = false;
  }

  isInTabletMode() {
    this.methodCalled('isInTabletMode');
    return Promise.resolve({tabletMode: this.isInTabletModeResponse});
  }

  makeTransparent() {
    this.methodCalled('makeTransparent');
  }
}

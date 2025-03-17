// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/result_image.js';

import type {SearchResultItem} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import type {HistoryEmbeddingsResultImageElement} from 'chrome://resources/cr_components/history_embeddings/result_image.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId, PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings-result-image', () => {
  let element: HistoryEmbeddingsResultImageElement;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;

  function generateResult(): SearchResultItem {
    return {
      title: 'Google',
      url: {url: 'http://google.com'},
      urlForDisplay: 'google.com',
      relativeTime: '2 hours ago',
      shortDateTime: 'Sept 2, 2022',
      sourcePassage: 'Google description',
      lastUrlVisitTimestamp: 1000,
      answerData: null,
      isUrlKnownToSync: false,
    };
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    imageServiceHandler = TestMock.fromClass(PageImageServiceHandlerRemote);
    PageImageServiceBrowserProxy.setInstance(
        new PageImageServiceBrowserProxy(imageServiceHandler));

    loadTimeData.overrideValues({enableHistoryEmbeddingsImages: true});
    element = document.createElement('cr-history-embeddings-result-image');
    document.body.appendChild(element);
  });

  test('ShowsSvgByDefault', () => {
    const svg = element.shadowRoot.querySelector('svg');
    assertTrue(!!svg);
  });

  test('RequestsImagesIfKnownToSync', async () => {
    imageServiceHandler.setResultFor(
        'getPageImageUrl', Promise.resolve({result: undefined}));

    // By default, results are not known to sync so they should not request
    // images.
    element.searchResult = generateResult();
    await element.updateComplete;
    assertEquals(0, imageServiceHandler.getCallCount('getPageImageUrl'));

    // Set a new result that is known to sync and verify that an image is
    // requested with the correct arguments.
    element.searchResult =
        Object.assign(generateResult(), {isUrlKnownToSync: true});
    await element.updateComplete;
    const requestArgs = await imageServiceHandler.whenCalled('getPageImageUrl');
    assertEquals(1, imageServiceHandler.getCallCount('getPageImageUrl'));
    assertEquals(PageImageServiceClientId.HistoryEmbeddings, requestArgs[0]);
    assertDeepEquals({url: 'http://google.com'}, requestArgs[1]);
    assertDeepEquals(
        {suggestImages: true, optimizationGuideImages: true}, requestArgs[2]);
  });

  test('ShowsImages', async () => {
    // Make the handler return an invalid response and verify that the image
    // is not set and is hidden.
    imageServiceHandler.setResultFor(
        'getPageImageUrl', Promise.resolve({result: undefined}));
    element.searchResult =
        Object.assign(generateResult(), {isUrlKnownToSync: true});
    await element.updateComplete;
    await imageServiceHandler.whenCalled('getPageImageUrl');

    let imageElement = element.shadowRoot.querySelector('#image')!;
    assertFalse(isVisible(imageElement));
    assertFalse(element.hasAttribute('has-image'));

    // Make the handler now return a valid image URL.
    imageServiceHandler.reset();
    const imageUrl = 'https://some-server/some-image.png';
    imageServiceHandler.setResultFor(
        'getPageImageUrl',
        Promise.resolve({result: {imageUrl: {url: imageUrl}}}));
    element.searchResult =
        Object.assign(generateResult(), {isUrlKnownToSync: true});
    await element.updateComplete;
    await imageServiceHandler.whenCalled('getPageImageUrl');
    await element.updateComplete;

    // Verify images are shown and set to the correct url.
    imageElement = element.shadowRoot.querySelector('#image')!;
    assertTrue(isVisible(imageElement));
    assertEquals(imageUrl, imageElement.getAttribute('auto-src'));
    assertTrue(element.hasAttribute('has-image'));
  });
});

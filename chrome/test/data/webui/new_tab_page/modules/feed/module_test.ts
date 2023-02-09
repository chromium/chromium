// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {FeedHandlerRemote} from 'chrome://new-tab-page/feed.mojom-webui.js';
import {FeedModuleElement, FeedProxy, feedV2Descriptor} from 'chrome://new-tab-page/lazy_load.js';
import {CrAutoImgElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesFeedModuleTest', () => {
  let handler: TestMock<FeedHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(FeedHandlerRemote, FeedProxy.setHandler);
  });

  test('module appears on render', async () => {
    handler.setResultFor('getFollowingFeedArticles', Promise.resolve((() => {
      const articles: Object[] = [];
      const exampleUrl = 'https://example.com/';
      for (let i = 0; i < 3; ++i) {
        articles.push({
          title: 'Title ' + i,
          publisher: 'Publisher ' + i,
          thumbnailUrl: {url: exampleUrl + 'thumbnail/' + i + '.png'},
          faviconUrl: {url: exampleUrl + 'favicon/' + i + '.ico'},
          url: {url: exampleUrl + i},
        });
      }
      return {articles: articles};
    })()));

    const module = await feedV2Descriptor.initialize(0) as FeedModuleElement;
    assertTrue(!!module);

    document.body.append(module);
    await handler.whenCalled('getFollowingFeedArticles');
    module.$.articleRepeat.render();
    const cards = Array.from(module.shadowRoot!.querySelectorAll('.card'));
    const urls =
        module.shadowRoot!.querySelectorAll<HTMLAnchorElement>('.card');

    assertTrue(isVisible(module.$.articles));
    assertEquals(3, cards.length);
    assertEquals(
        'Title 1', cards[1]!.querySelector<HTMLElement>('.title')!.textContent);
    assertEquals(
        'Publisher 1',
        cards[1]!.querySelector<HTMLElement>('.info-text')!.textContent);
    assertEquals(
        'https://example.com/thumbnail/1.png',
        cards[1]!.querySelector<CrAutoImgElement>('.thumbnail')!.autoSrc);
    assertEquals(
        'https://example.com/favicon/1.ico',
        cards[1]!.querySelector<CrAutoImgElement>('.favicon')!.autoSrc);
    assertEquals('https://example.com/0', urls[0]!.href);
    assertEquals('https://example.com/1', urls[1]!.href);
  });

  test('empty module shows without data', async () => {
    handler.setResultFor(
        'getFollowingFeedArticles', Promise.resolve({articles: []}));

    const module = await feedV2Descriptor.initialize(0);
    await handler.whenCalled('getFollowingFeedArticles');
    assertTrue(!!module);
  });
});

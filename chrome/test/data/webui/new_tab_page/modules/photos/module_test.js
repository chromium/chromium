// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, photosDescriptor, PhotosProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://test/new_tab_page/metrics_test_support.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {isVisible} from 'chrome://test/test_util.js';

suite('NewTabPageModulesPhotosModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  /** @type {!MetricsTracker} */
  let metrics;

  setup(() => {
    document.body.innerHTML = '';
    handler =
        installMock(photos.mojom.PhotosHandlerRemote, PhotosProxy.setHandler);
    metrics = fakeMetricsPrivate();
  });

  test('module appears on render', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'}
        }
      ]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', false));
    module.dispatchEvent(new Event('detect-impression'));

    // Assert.
    const items =
        Array.from(module.shadowRoot.querySelectorAll('#memories > .memory'));
    assertTrue(!!module);
    assertTrue(isVisible(module.$.memories));
    assertEquals(2, items.length);
    assertEquals(
        'Title 1', items[0].querySelector('.memory-title').textContent);
    assertEquals(
        'Title 2', items[1].querySelector('.memory-title').textContent);
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', false));
  });

  test('module does not show without data', async () => {
    // Arrange.
    handler.setResultFor('getMemories', Promise.resolve({memories: []}));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = await photosDescriptor.initialize(0);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Assert.
    assertFalse(!!module);
  });

  test('info button click opens info dialog', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'}
        }
      ]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Act.
    $$(module, 'ntp-module-header')
        .dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertTrue(!!$$(module, 'ntp-info-dialog'));
  });

  test('backend is notified when module is dismissed or restored', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'}
        }
      ]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Act.
    const dismiss = {event: null};
    module.addEventListener('dismiss-module', (e) => dismiss.event = e);
    $$(module, 'ntp-module-header')
        .dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    assertEquals(
        loadTimeData.getString('modulesPhotosMemoriesHiddenToday'),
        dismiss.event.detail.message);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    dismiss.event.detail.restoreCallback();

    // Assert.
    assertEquals(1, handler.getCallCount('restoreModule'));
  });

  test('backend is notified when module is disabled', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'}
        }
      ]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Act.
    const disable = {event: null};
    module.addEventListener('disable-module', (e) => disable.event = e);
    $$(module, 'ntp-module-header')
        .dispatchEvent(new Event('disable-button-click'));

    // Assert.
    assertEquals(
        loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesPhotosMemoriesDisabled')),
        disable.event.detail.message);
  });

  test('explore card is shown when 1 memory', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
      }]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Assert.
    assertTrue(!!$$(module, '#exploreCard'));
  });

  test('shows only up to 3 memories', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'}
        },
        {
          title: 'Title 3',
          id: 'key3',
          coverUrl: {url: 'https://fakeurl.com/3?token=foo'}
        },
        {
          title: 'Title 4',
          id: 'key4',
          coverUrl: {url: 'https://fakeurl.com/4?token=foo'}
        }
      ]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Assert.
    const items =
        Array.from(module.shadowRoot.querySelectorAll('#memories > .memory'));
    assertEquals(3, items.length);
  });

  test('backend is notified when user opt out', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
      }]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', true));
    module.dispatchEvent(new Event('detect-impression'));

    // Asserts.
    assertTrue(!!$$(module, '#optInCard'));
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', true));

    // Act.
    const disable = {event: null};
    module.addEventListener('disable-module', (e) => disable.event = e);
    $$(module, '#optOutButton').click();

    // Asserts.
    assertEquals(1, handler.getCallCount('onUserOptIn'));
    assertEquals(false, handler.getArgs('onUserOptIn')[0]);
    assertEquals(
        loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesPhotosMemoriesDisabled')),
        disable.event.detail.message);
    assertEquals(1, metrics.count('NewTabPage.Photos.UserOptIn', false));
  });

  test('UI is updated and backend notified when user opt in', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'}
      }]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', true));
    module.dispatchEvent(new Event('detect-impression'));

    // Asserts.
    assertTrue(!!$$(module, '#optInCard'));
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', true));

    // Act.
    $$(module, '#optInButton').click();
    $$(module, '#welcomeCardElement').render();
    $$(module, '#memoriesElement').render();
    $$(module, '#exploreCardElement').render();

    // Asserts.
    assertFalse(isVisible(assert($$(module, '#optInCard'))));
    assertEquals(1, handler.getCallCount('onUserOptIn'));
    assertEquals(true, handler.getArgs('onUserOptIn')[0]);
    const items =
        Array.from(module.shadowRoot.querySelectorAll('#memories > .memory'));
    assertEquals(1, items.length);
    assertEquals(1, metrics.count('NewTabPage.Photos.UserOptIn', true));
  });

  test('click on memory trigger proper logging and pref change', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        itemUrl: {url: '#'}
      }]
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    const module = assert(await photosDescriptor.initialize(0));
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');

    // Act.
    const usage = {event: null};
    module.addEventListener('usage', (e) => usage.event = e);
    $$(module, '#memories > .memory').click();

    // Assert.
    assertEquals(1, handler.getCallCount('onMemoryOpen'));
    assertTrue(!!usage.event);
  });
});

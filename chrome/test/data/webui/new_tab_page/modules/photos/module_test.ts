// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {DisableModuleEvent, DismissModuleEvent, photosDescriptor, PhotosModuleElement, PhotosProxy} from 'chrome://new-tab-page/lazy_load.js';
import {$$, DomIf} from 'chrome://new-tab-page/new_tab_page.js';
import {PhotosHandlerRemote} from 'chrome://new-tab-page/photos.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

suite('NewTabPageModulesPhotosModuleTest', () => {
  let handler: TestBrowserProxy<PhotosHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(PhotosHandlerRemote, PhotosProxy.setHandler);
    metrics = fakeMetricsPrivate();
  });

  suite('svg-split-enabled-boy-dog-image', () => {
    setup(() => {
      loadTimeData.overrideValues({
        photosModuleCustomArtWork: '1',
        photosModuleSplitSvgCustomArtWork: true,
      });
    });

    test('artwork with constituent images of boy and dog shown', async () => {
      // Arrange.
      const data = {
        memories: [
          {
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
          },
          {
            title: 'Title 2',
            id: 'key2',
            coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
          },
        ],
      };
      handler.setResultFor('getMemories', Promise.resolve(data));
      handler.setResultFor(
          'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
      handler.setResultFor(
          'shouldShowSoftOptOutButton',
          Promise.resolve({showSoftOptOutButton: false}));
      handler.setResultFor(
          'getOptInTitleText',
          Promise.resolve({optInTitleText: 'See your memories here'}));
      const module =
          await photosDescriptor.initialize(0) as PhotosModuleElement;
      assertTrue(!!module);
      document.body.append(module);
      await handler.whenCalled('getMemories');
      await handler.whenCalled('shouldShowOptInScreen');
      await handler.whenCalled('shouldShowSoftOptOutButton');
      await handler.whenCalled('getOptInTitleText');

      const boyDogCustomArtWork =
          module.shadowRoot!.querySelector('#boyDogcustomArtWork');
      assertTrue(!!boyDogCustomArtWork);

      // All other artworks are not shown
      const lakeBoycustomArtWork =
          module.shadowRoot!.querySelector('#lakeBoycustomArtWork');
      assertTrue(!lakeBoycustomArtWork);

      const illustrationsCustomArtWork =
          module.shadowRoot!.querySelector('#illustrationsCustomArtWork');
      assertTrue(!illustrationsCustomArtWork);

      const defaultArtWork =
          module.shadowRoot!.querySelector('#defaultOpInArtWork');
      assertTrue(!defaultArtWork);
    });
  });

  suite('svg-split-enabled-lake-boy-image', () => {
    setup(() => {
      loadTimeData.overrideValues({
        photosModuleCustomArtWork: '2',
        photosModuleSplitSvgCustomArtWork: true,
      });
    });

    test('artwork with constituent images of lake and boy shown', async () => {
      // Arrange.
      const data = {
        memories: [
          {
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
          },
          {
            title: 'Title 2',
            id: 'key2',
            coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
          },
        ],
      };
      handler.setResultFor('getMemories', Promise.resolve(data));
      handler.setResultFor(
          'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
      handler.setResultFor(
          'shouldShowSoftOptOutButton',
          Promise.resolve({showSoftOptOutButton: false}));
      handler.setResultFor(
          'getOptInTitleText',
          Promise.resolve({optInTitleText: 'See your memories here'}));
      const module =
          await photosDescriptor.initialize(0) as PhotosModuleElement;
      assertTrue(!!module);
      document.body.append(module);
      await handler.whenCalled('getMemories');
      await handler.whenCalled('shouldShowOptInScreen');
      await handler.whenCalled('shouldShowSoftOptOutButton');
      await handler.whenCalled('getOptInTitleText');

      const lakeBoycustomArtWork =
          module.shadowRoot!.querySelector('#lakeBoycustomArtWork');
      assertTrue(!!lakeBoycustomArtWork);

      // All other artworks are not shown
      const boyDogCustomArtWork =
          module.shadowRoot!.querySelector('#boyDogcustomArtWork');
      assertTrue(!boyDogCustomArtWork);

      const illustrationsCustomArtWork =
          module.shadowRoot!.querySelector('#illustrationsCustomArtWork');
      assertTrue(!illustrationsCustomArtWork);

      const defaultArtWork =
          module.shadowRoot!.querySelector('#defaultOpInArtWork');
      assertTrue(!defaultArtWork);
    });
  });

  suite('svg-split-enabled-illustrations-image', () => {
    setup(() => {
      loadTimeData.overrideValues({
        photosModuleCustomArtWork: '3',
        photosModuleSplitSvgCustomArtWork: true,
      });
    });

    test('artwork with constituent images of illustrations shown', async () => {
      // Arrange.
      const data = {
        memories: [
          {
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
          },
          {
            title: 'Title 2',
            id: 'key2',
            coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
          },
        ],
      };
      handler.setResultFor('getMemories', Promise.resolve(data));
      handler.setResultFor(
          'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
      handler.setResultFor(
          'shouldShowSoftOptOutButton',
          Promise.resolve({showSoftOptOutButton: false}));
      handler.setResultFor(
          'getOptInTitleText',
          Promise.resolve({optInTitleText: 'See your memories here'}));
      const module =
          await photosDescriptor.initialize(0) as PhotosModuleElement;
      assertTrue(!!module);
      document.body.append(module);
      await handler.whenCalled('getMemories');
      await handler.whenCalled('shouldShowOptInScreen');
      await handler.whenCalled('shouldShowSoftOptOutButton');
      await handler.whenCalled('getOptInTitleText');

      const illustrationsCustomArtWork =
          module.shadowRoot!.querySelector('#illustrationsCustomArtWork');
      assertTrue(!!illustrationsCustomArtWork);

      // All other artworks are not shown
      const boyDogCustomArtWork =
          module.shadowRoot!.querySelector('#boyDogcustomArtWork');
      assertTrue(!boyDogCustomArtWork);

      const lakeBoycustomArtWork =
          module.shadowRoot!.querySelector('#lakeBoycustomArtWork');
      assertTrue(!lakeBoycustomArtWork);

      const defaultArtWork =
          module.shadowRoot!.querySelector('#defaultOpInArtWork');
      assertTrue(!defaultArtWork);
    });
  });

  suite('svg-split-enabled-default-image', () => {
    setup(() => {
      loadTimeData.overrideValues({
        photosModuleCustomArtWork: '4',
        photosModuleSplitSvgCustomArtWork: true,
      });
    });

    test(
        'default artwork shown constituent images design is not implemented',
        async () => {
          // Arrange.
          const data = {
            memories: [
              {
                title: 'Title 1',
                id: 'key1',
                coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
              },
              {
                title: 'Title 2',
                id: 'key2',
                coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
              },
            ],
          };
          handler.setResultFor('getMemories', Promise.resolve(data));
          handler.setResultFor(
              'shouldShowOptInScreen',
              Promise.resolve({showOptInScreen: true}));
          handler.setResultFor(
              'shouldShowSoftOptOutButton',
              Promise.resolve({showSoftOptOutButton: false}));
          handler.setResultFor(
              'getOptInTitleText',
              Promise.resolve({optInTitleText: 'See your memories here'}));
          const module =
              await photosDescriptor.initialize(0) as PhotosModuleElement;
          assertTrue(!!module);
          document.body.append(module);
          await handler.whenCalled('getMemories');
          await handler.whenCalled('shouldShowOptInScreen');
          await handler.whenCalled('shouldShowSoftOptOutButton');
          await handler.whenCalled('getOptInTitleText');

          const defaultArtWork =
              module.shadowRoot!.querySelector('#defaultOpInArtWork');
          assertTrue(!!defaultArtWork);

          // All other artworks are not shown
          const boyDogCustomArtWork =
              module.shadowRoot!.querySelector('#boyDogcustomArtWork');
          assertTrue(!boyDogCustomArtWork);

          const lakeBoycustomArtWork =
              module.shadowRoot!.querySelector('#lakeBoycustomArtWork');
          assertTrue(!lakeBoycustomArtWork);

          const illustrationsCustomArtWork =
              module.shadowRoot!.querySelector('#illustrationsCustomArtWork');
          assertTrue(!illustrationsCustomArtWork);
        });
  });

  suite('custom-artwork-enabled', () => {
    setup(() => {
      loadTimeData.overrideValues({
        photosModuleCustomArtWork: '1',
        photosModuleSplitSvgCustomArtWork: false,
      });
    });

    test(
        'custom art work is shown when custom artwork flag is set',
        async () => {
          // Arrange.
          const data = {
            memories: [
              {
                title: 'Title 1',
                id: 'key1',
                coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
              },
              {
                title: 'Title 2',
                id: 'key2',
                coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
              },
            ],
          };
          handler.setResultFor('getMemories', Promise.resolve(data));
          handler.setResultFor(
              'shouldShowOptInScreen',
              Promise.resolve({showOptInScreen: true}));
          handler.setResultFor(
              'shouldShowSoftOptOutButton',
              Promise.resolve({showSoftOptOutButton: false}));
          handler.setResultFor(
              'getOptInTitleText',
              Promise.resolve({optInTitleText: 'See your memories here'}));
          const module =
              await photosDescriptor.initialize(0) as PhotosModuleElement;
          assertTrue(!!module);
          document.body.append(module);
          await handler.whenCalled('getMemories');
          await handler.whenCalled('shouldShowOptInScreen');
          await handler.whenCalled('shouldShowSoftOptOutButton');
          await handler.whenCalled('getOptInTitleText');

          const img = module.shadowRoot!.querySelector('#customArtWork img');
          assertTrue(!!img);
          assertEquals(
              'chrome://new-tab-page/modules/photos/images/img01_240x236.svg',
              img.getAttribute('src'));

          const defaultArtWork =
              module.shadowRoot!.querySelector('#defaultOpInArtWork');
          assertTrue(!defaultArtWork);
        });
  });

  test(
      'default artwork is shown when when custom artwork flag is not set',
      async () => {
        // Arrange.
        const data = {
          memories: [
            {
              title: 'Title 1',
              id: 'key1',
              coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
            },
            {
              title: 'Title 2',
              id: 'key2',
              coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
            },
          ],
        };
        handler.setResultFor('getMemories', Promise.resolve(data));
        handler.setResultFor(
            'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
        handler.setResultFor(
            'shouldShowSoftOptOutButton',
            Promise.resolve({showSoftOptOutButton: false}));
        handler.setResultFor(
            'getOptInTitleText',
            Promise.resolve({optInTitleText: 'See your memories here'}));
        const module =
            await photosDescriptor.initialize(0) as PhotosModuleElement;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getMemories');
        await handler.whenCalled('shouldShowOptInScreen');
        await handler.whenCalled('shouldShowSoftOptOutButton');
        await handler.whenCalled('getOptInTitleText');

        const img = module.shadowRoot!.querySelector('#customArtWork');
        assertTrue(!img);

        const defaultArtWork =
            module.shadowRoot!.querySelector('#defaultOpInArtWork');
        assertTrue(!!defaultArtWork);
      });

  test('module appears on render', async () => {
    // Arrange.
    const data = {
      memories: [
        {
          title: 'Title 1',
          id: 'key1',
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
        },
      ],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', false));
    module.dispatchEvent(new Event('detect-impression'));

    // Assert.
    const items = module.shadowRoot!.querySelectorAll('#memories > .memory');
    assertTrue(!!module);
    assertTrue(isVisible(module.$.memories));
    assertEquals(2, items.length);
    assertEquals(
        'Title 1', items[0]!.querySelector('.memory-title')!.textContent);
    assertEquals(
        'Title 2', items[1]!.querySelector('.memory-title')!.textContent);
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', false));
  });

  test('module does not show without data', async () => {
    // Arrange.
    handler.setResultFor('getMemories', Promise.resolve({memories: []}));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

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
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
        },
      ],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Act.
    ($$(module, 'ntp-module-header')!
     ).dispatchEvent(new Event('info-button-click'));

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
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
        },
      ],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Act.
    const whenFired = eventToPromise('dismiss-module', module);
    ($$(module, 'ntp-module-header')!
     ).dispatchEvent(new Event('dismiss-button-click'));

    // Assert.
    const event: DismissModuleEvent = await whenFired;
    assertEquals(
        loadTimeData.getString('modulesPhotosMemoriesHiddenToday'),
        event.detail.message);
    assertEquals(1, handler.getCallCount('dismissModule'));

    // Act.
    event.detail.restoreCallback();

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
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
        },
      ],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Act.
    const whenFired = eventToPromise('disable-module', module);
    ($$(module, 'ntp-module-header')!
     ).dispatchEvent(new Event('disable-button-click'));

    // Assert.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(
        loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesPhotosMemoriesDisabled')),
        event.detail.message);
  });

  test('explore card is shown when 1 memory', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

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
          coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        },
        {
          title: 'Title 2',
          id: 'key2',
          coverUrl: {url: 'https://fakeurl.com/2?token=foo'},
        },
        {
          title: 'Title 3',
          id: 'key3',
          coverUrl: {url: 'https://fakeurl.com/3?token=foo'},
        },
        {
          title: 'Title 4',
          id: 'key4',
          coverUrl: {url: 'https://fakeurl.com/4?token=foo'},
        },
      ],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Assert.
    const items = module.shadowRoot!.querySelectorAll('#memories > .memory');
    assertEquals(3, items.length);
  });

  test('backend is notified when user opt out', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', true));
    module.dispatchEvent(new Event('detect-impression'));

    // Asserts.
    assertTrue(!!$$(module, '#optInCard'));
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', true));

    // Act.
    const whenFired = eventToPromise('disable-module', module);
    $$<HTMLElement>(module, '#optOutButton')!.click();

    // Asserts.
    const event: DisableModuleEvent = await whenFired;
    assertEquals(1, handler.getCallCount('onUserOptIn'));
    assertEquals(false, handler.getArgs('onUserOptIn')[0]);
    assertEquals(
        loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesPhotosMemoriesDisabled')),
        event.detail.message);
    assertEquals(1, metrics.count('NewTabPage.Photos.UserOptIn', 0));
  });

  test('UI is updated and backend notified when user opt in', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', true));
    module.dispatchEvent(new Event('detect-impression'));

    // Asserts.
    assertTrue(!!$$(module, '#optInCard'));
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', true));

    // Act.
    $$<HTMLElement>(module, '#optInButton')!.click();
    module.$.welcomeCardElement.render();
    module.$.memoriesElement.render();
    $$<DomIf>(module, '#exploreCardElement')!.render();

    // Asserts.
    assertFalse(isVisible($$(module, '#optInCard')));
    assertEquals(1, handler.getCallCount('onUserOptIn'));
    assertEquals(true, handler.getArgs('onUserOptIn')[0]);
    const items = module.shadowRoot!.querySelectorAll('#memories > .memory');
    assertEquals(1, items.length);
    assertEquals(1, metrics.count('NewTabPage.Photos.UserOptIn', 1));
  });

  test('click on memory trigger proper logging and pref change', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        itemUrl: {url: '#'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Act.
    const whenFired = eventToPromise('usage', module);
    $$<HTMLElement>(module, '#memories > .memory')!.click();

    // Assert.
    const event = await whenFired;
    assertEquals(1, handler.getCallCount('onMemoryOpen'));
    assertTrue(!!event);
  });

  test(
      'soft opt out button is shown when soft opt out is enabled', async () => {
        // Arrange.
        const data = {
          memories: [{
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
            itemUrl: {url: '#'},
          }],
        };
        handler.setResultFor('getMemories', Promise.resolve(data));
        handler.setResultFor(
            'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
        handler.setResultFor(
            'shouldShowSoftOptOutButton',
            Promise.resolve({showSoftOptOutButton: true}));
        handler.setResultFor(
            'getOptInTitleText',
            Promise.resolve({optInTitleText: 'See your memories here'}));

        const module =
            await photosDescriptor.initialize(0) as PhotosModuleElement;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getMemories');
        await handler.whenCalled('shouldShowOptInScreen');
        await handler.whenCalled('shouldShowSoftOptOutButton');
        await handler.whenCalled('getOptInTitleText');

        // Assert.
        assertTrue(isVisible($$(module, '#softOptOutButton')));
        assertTrue(!!$$(module, 'ntp-module-header'));
        assertFalse($$(module, 'ntp-module-header')!.hideMenuButton);
        assertFalse($$(module, 'ntp-module-header')!.showDismissButton);
      });

  test(
      'menu is not shown in opt-in card when soft opt out is disabled',
      async () => {
        // Arrange.
        const data = {
          memories: [{
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
            itemUrl: {url: '#'},
          }],
        };
        handler.setResultFor('getMemories', Promise.resolve(data));
        handler.setResultFor(
            'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
        handler.setResultFor(
            'shouldShowSoftOptOutButton',
            Promise.resolve({showSoftOptOutButton: false}));
        handler.setResultFor(
            'getOptInTitleText',
            Promise.resolve({optInTitleText: 'See your memories here'}));

        const module =
            await photosDescriptor.initialize(0) as PhotosModuleElement;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getMemories');
        await handler.whenCalled('shouldShowOptInScreen');
        await handler.whenCalled('shouldShowSoftOptOutButton');
        await handler.whenCalled('getOptInTitleText');

        // Assert.
        assertTrue(!!$$(module, 'ntp-module-header'));
        assertTrue($$(module, 'ntp-module-header')!.hideMenuButton);
      });

  test('menu is shown in memories card', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
        itemUrl: {url: '#'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');
    // Assert.
    assertTrue(!!$$(module, 'ntp-module-header'));
    assertFalse($$(module, 'ntp-module-header')!.hideMenuButton);
    assertTrue($$(module, 'ntp-module-header')!.showDismissButton);
  });

  test(
      'menu is shown after user opts-in from softOptOut disabled card',
      async () => {
        // Arrange.
        const data = {
          memories: [{
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
            itemUrl: {url: '#'},
          }],
        };
        handler.setResultFor('getMemories', Promise.resolve(data));
        handler.setResultFor(
            'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
        handler.setResultFor(
            'shouldShowSoftOptOutButton',
            Promise.resolve({showSoftOptOutButton: false}));
        handler.setResultFor(
            'getOptInTitleText',
            Promise.resolve({optInTitleText: 'See your memories here'}));

        const module =
            await photosDescriptor.initialize(0) as PhotosModuleElement;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getMemories');
        await handler.whenCalled('shouldShowOptInScreen');
        await handler.whenCalled('shouldShowSoftOptOutButton');
        await handler.whenCalled('getOptInTitleText');

        // Menu is not shown because softOptOut is disabled
        assertTrue(!!$$(module, 'ntp-module-header'));
        assertTrue($$(module, 'ntp-module-header')!.hideMenuButton);

        $$<HTMLElement>(module, '#optInButton')!.click();

        // Menu is shown in memories card
        assertFalse($$(module, 'ntp-module-header')!.hideMenuButton);
        assertTrue($$(module, 'ntp-module-header')!.showDismissButton);
      });

  test(
      'menu with Dismiss module is shown after user opts-in from softOptOut enabled card',
      async () => {
        // Arrange.
        const data = {
          memories: [{
            title: 'Title 1',
            id: 'key1',
            coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
            itemUrl: {url: '#'},
          }],
        };
        handler.setResultFor('getMemories', Promise.resolve(data));
        handler.setResultFor(
            'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
        handler.setResultFor(
            'shouldShowSoftOptOutButton',
            Promise.resolve({showSoftOptOutButton: true}));
        handler.setResultFor(
            'getOptInTitleText',
            Promise.resolve({optInTitleText: 'See your memories here'}));

        const module =
            await photosDescriptor.initialize(0) as PhotosModuleElement;
        assertTrue(!!module);
        document.body.append(module);
        await handler.whenCalled('getMemories');
        await handler.whenCalled('shouldShowOptInScreen');
        await handler.whenCalled('shouldShowSoftOptOutButton');
        await handler.whenCalled('getOptInTitleText');

        // Menu is shown because softOptOut is enabled
        assertTrue(!!$$(module, 'ntp-module-header'));
        assertFalse($$(module, 'ntp-module-header')!.hideMenuButton);
        assertFalse($$(module, 'ntp-module-header')!.showDismissButton);

        $$<HTMLElement>(module, '#optInButton')!.click();

        // Menu is shown in memories card
        assertFalse($$(module, 'ntp-module-header')!.hideMenuButton);
        assertTrue($$(module, 'ntp-module-header')!.showDismissButton);
      });

  test('backend is notified when user soft opt outs', async () => {
    // Arrange.
    const data = {
      memories: [{
        title: 'Title 1',
        id: 'key1',
        coverUrl: {url: 'https://fakeurl.com/1?token=foo'},
      }],
    };
    handler.setResultFor('getMemories', Promise.resolve(data));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: true}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: true}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    assertEquals(0, metrics.count('NewTabPage.Photos.ModuleShown', true));
    module.dispatchEvent(new Event('detect-impression'));

    // Asserts.
    assertTrue(!!$$(module, '#optInCard'));
    assertEquals(1, metrics.count('NewTabPage.Photos.ModuleShown', true));

    // Act.
    const whenFired = eventToPromise('dismiss-module', module);
    $$<HTMLElement>(module, '#softOptOutButton')!.click();

    // Asserts.
    const event: DismissModuleEvent = await whenFired;
    assertEquals(1, handler.getCallCount('softOptOut'));
    assertEquals(
        loadTimeData.getString('modulesPhotosMemoriesSoftOptOut'),
        event.detail.message);
    assertEquals(1, metrics.count('NewTabPage.Photos.UserOptIn', 2));
  });

  test('module does not show when user soft opted out', async () => {
    // Arrange.
    handler.setResultFor('getMemories', Promise.resolve({memories: []}));
    handler.setResultFor(
        'shouldShowOptInScreen', Promise.resolve({showOptInScreen: false}));
    handler.setResultFor(
        'shouldShowSoftOptOutButton',
        Promise.resolve({showSoftOptOutButton: false}));
    handler.setResultFor(
        'getOptInTitleText',
        Promise.resolve({optInTitleText: 'See your memories here'}));
    const module = await photosDescriptor.initialize(0) as PhotosModuleElement;
    await handler.whenCalled('getMemories');
    await handler.whenCalled('shouldShowOptInScreen');
    await handler.whenCalled('shouldShowSoftOptOutButton');
    await handler.whenCalled('getOptInTitleText');

    // Assert.
    assertFalse(!!module);
  });
});

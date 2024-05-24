// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AmbientObserver, AmbientPreviewLargeElement, Paths, PersonalizationRouterElement, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


suite('AmbientPreviewLargeElementTest', function() {
  let ambientPreviewLargeElement: AmbientPreviewLargeElement|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouterElement.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouterElement);

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouterElement.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientPreviewLargeElement);
    ambientPreviewLargeElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouterElement.instance = routerOriginal;
  });

  test(
      'displays zero state message when ambient mode is disabled', async () => {
        loadTimeData.overrideValues({isAmbientModeAllowed: true});
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.previews = ambientProvider.previews;
        ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewLargeElement);

        const messageContainer =
            ambientPreviewLargeElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const textSpan = messageContainer.querySelector<HTMLSpanElement>(
            '#turnOnDescription');
        assertTrue(!!textSpan);
        assertEquals(
            ambientPreviewLargeElement.i18n(
                'ambientModeMainPageZeroStateMessageV2'),
            textSpan.innerText.trim());
      });

  test(
      'clicks turn on button enables ambient mode and navigates to ambient subpage',
      async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.previews = ambientProvider.previews;
        ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewLargeElement);

        const messageContainer =
            ambientPreviewLargeElement.shadowRoot!.getElementById(
                'messageContainer');
        assertTrue(!!messageContainer);
        const button = messageContainer.querySelector('cr-button');
        assertTrue(!!button);

        personalizationStore.setReducersEnabled(true);
        button.click();
        assertTrue(personalizationStore.data.ambient.ambientModeEnabled);

        const original = PersonalizationRouterElement.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouterElement.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouterElement.instance = original;
              },
            } as PersonalizationRouterElement;
          };
        });
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.AMBIENT, path);
        assertDeepEquals({}, queryParams);
      });

  test('click big image preview goes to ambient subpage', async () => {
    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      previews: ambientProvider.previews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
      };
    });

    ambientPreviewLargeElement.shadowRoot!
        .querySelector<HTMLImageElement>('.preview-image.clickable')!.click();

    const [path, queryParams] = await goToRoutePromise;

    assertEquals(Paths.AMBIENT, path, 'navigates to ambient subpage');
    assertDeepEquals({}, queryParams, 'no query params set');
  });

  test('shows 2 or 3 preview images', async () => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      previews: ambientProvider.previews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    assertEquals(
        'http://test_url2',
        ambientPreviewLargeElement.shadowRoot!
            .querySelector<HTMLImageElement>(
                '#imageContainer .preview-image')!.getAttribute('auto-src'),
        'large container shows album preview image from first selected album');

    const thumbnailContainer =
        ambientPreviewLargeElement.shadowRoot!.getElementById(
            'thumbnailContainer');
    assertTrue(!!thumbnailContainer, 'thumbnailContainer exists');

    let thumbnailImages =
        thumbnailContainer.querySelectorAll<HTMLImageElement>('img');
    assertDeepEquals(
        [
          'http://preview0',
          'http://preview1',
        ],
        Array.from(thumbnailImages).map(img => img.getAttribute('auto-src')),
        'first two preview images are shown for kArtGallery');

    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    thumbnailImages =
        thumbnailContainer.querySelectorAll<HTMLImageElement>('img');
    assertDeepEquals(
        [
          'http://preview0',
          'http://preview1',
          'http://preview2',
        ],
        Array.from(thumbnailImages).map(img => img.getAttribute('auto-src')),
        'first three preview images are shown for kGooglePhotos');
  });

  test('click ambient thumbnail goes to ambient subpage', async () => {
    personalizationStore.data.ambient = {
      ...personalizationStore.data.ambient,
      albums: ambientProvider.albums,
      topicSource: TopicSource.kArtGallery,
      ambientModeEnabled: true,
      previews: ambientProvider.previews,
    };
    ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {
            scrollTo: 'topic-source-list',
          }) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
      };
    });

    ambientPreviewLargeElement.shadowRoot!.getElementById(
                                              'thumbnailContainer')!.click();

    const [path, queryParams] = await goToRoutePromise;

    assertEquals(Paths.AMBIENT, path, 'navigates to ambient subpage');
    assertDeepEquals(
        {scrollTo: 'topic-source-list'}, queryParams, 'query params set');
  });

  test('displays not available message for non-allowed user', async () => {
    // Disable `isAmbientModeAllowed` to mock an enterprise controlled user.
    loadTimeData.overrideValues({isAmbientModeAllowed: false});

    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
    personalizationStore.data.ambient.ambientModeEnabled = false;
    personalizationStore.data.ambient.previews = ambientProvider.previews;
    ambientPreviewLargeElement = initElement(AmbientPreviewLargeElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewLargeElement);

    const messageContainer =
        ambientPreviewLargeElement.shadowRoot!.getElementById(
            'messageContainer');
    assertTrue(!!messageContainer);
    const textSpan =
        messageContainer.querySelector<HTMLSpanElement>('#turnOnDescription');
    assertTrue(!!textSpan);
    assertEquals(
        ambientPreviewLargeElement.i18n(
            'ambientModeMainPageEnterpriseUserMessage'),
        textSpan.innerText.trim());
  });
});

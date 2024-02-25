// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AmbientObserver, AmbientPreviewSmallElement, PersonalizationRouterElement, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


suite('AmbientPreviewSmallElementTest', function() {
  let ambientPreviewSmallElement: AmbientPreviewSmallElement|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouterElement.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouterElement);

  setup(() => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouterElement.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientPreviewSmallElement);
    ambientPreviewSmallElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouterElement.instance = routerOriginal;
  });

  test(
      'displays zero state message when ambient mode is disabled', async () => {
        personalizationStore.data.ambient.albums = ambientProvider.albums;
        personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.data.ambient.previews = ambientProvider.previews;
        ambientPreviewSmallElement = initElement(AmbientPreviewSmallElement);
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientPreviewSmallElement);

        const zeroStateTextContainer =
            ambientPreviewSmallElement.shadowRoot!.getElementById(
                'zeroStateTextContainer');
        assertTrue(!!zeroStateTextContainer);
        const textSpan =
            zeroStateTextContainer.firstElementChild as HTMLSpanElement;
        assertTrue(!!textSpan);
        assertEquals('span', textSpan.tagName.toLowerCase());
        assertEquals(
            ambientPreviewSmallElement.i18n(
                'ambientModeMainPageZeroStateMessage'),
            textSpan.innerText.trim());
      });

  test('shows placeholders while loading', async () => {
    // Null indicates that this value has not yet loaded.
    personalizationStore.data.ambient.ambientModeEnabled = null;
    ambientPreviewSmallElement = initElement(AmbientPreviewSmallElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewSmallElement);

    const container = ambientPreviewSmallElement.$.container;
    assertEquals('imagePlaceholder', container.firstElementChild?.id);

    const textPlaceholder = container.querySelector('#textPlaceholder');
    assertTrue(!!textPlaceholder, 'textPlaceholder element exists');
    for (const child of textPlaceholder.children) {
      assertTrue(
          child.classList.contains('placeholder'),
          'every element has placeholder class');
    }

    assertEquals(null, container.querySelector('#imageContainer'));
  });

  test('shows placeholders while waiting for assets to load', async () => {
    // Only AmbientModeEnabled is set.
    personalizationStore.data.ambient.ambientModeEnabled = true;
    // Null indicates that albums have not yet loaded.
    personalizationStore.data.ambient.albums = null;
    ambientPreviewSmallElement = initElement(AmbientPreviewSmallElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewSmallElement);

    const container = ambientPreviewSmallElement.$.container;
    assertEquals('imagePlaceholder', container.firstElementChild?.id);

    const textPlaceholder = container.querySelector('#textPlaceholder');
    assertTrue(!!textPlaceholder, 'textPlaceholder element exists');
    for (const child of textPlaceholder.children) {
      assertTrue(
          child.classList.contains('placeholder'),
          'every element has placeholder class');
    }

    assertEquals(null, container.querySelector('#imageContainer'));
  });

  test('ends loading early if ambient mode is disabled', async () => {
    personalizationStore.data.ambient.ambientModeEnabled = false;
    ambientPreviewSmallElement = initElement(AmbientPreviewSmallElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewSmallElement);

    const zeroStateTextContainer =
        ambientPreviewSmallElement.shadowRoot!.getElementById(
            'zeroStateTextContainer');
    assertTrue(!!zeroStateTextContainer);
    const textSpan =
        zeroStateTextContainer.firstElementChild as HTMLSpanElement;
    assertTrue(!!textSpan);
    assertEquals('span', textSpan.tagName.toLowerCase());
    assertEquals(
        ambientPreviewSmallElement.i18n('ambientModeMainPageZeroStateMessage'),
        textSpan.innerText.trim());
  });

  test('shows image when loaded', async () => {
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kArtGallery;
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.previews = ambientProvider.previews;
    ambientPreviewSmallElement = initElement(AmbientPreviewSmallElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientPreviewSmallElement);

    const imageContainer =
        ambientPreviewSmallElement.shadowRoot?.getElementById('imageContainer');
    assertTrue(!!imageContainer, 'imageContainer exists');
    assertEquals(
        1, imageContainer.getElementsByTagName('img').length,
        '1 img element displayed');
  });
});

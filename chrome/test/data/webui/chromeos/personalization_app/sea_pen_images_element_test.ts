// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenImagesElement, SparklePlaceholderElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenImagesElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let seaPenImagesElement: SeaPenImagesElement|null;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenImagesElement);
    seaPenImagesElement = null;
  });

  test('displays thumbnail placeholders', async () => {
    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholder =
        seaPenImagesElement.shadowRoot!.querySelector('.placeholder');
    assertFalse(
        !!loadingThumbnailPlaceholder,
        'thumbnails should not be in loading state');

    const thumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .thumbnail-placeholder');
    assertEquals(
        4, thumbnailPlaceholders!.length,
        'should be 4 placeholders available.');
  });

  test('displays loading thumbnail placeholders', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailsLoading = true;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!
            .querySelectorAll<SparklePlaceholderElement>(
                'div:not([hidden]) sparkle-placeholder-element');
    assertEquals(
        4, loadingThumbnailPlaceholders!.length,
        'should be 4 loading placeholders available.');
  });

  test('displays image thumbnails', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailsLoading = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const thumbnails = seaPenImagesElement.shadowRoot!.querySelectorAll(
        'div:not([hidden]).thumbnail-item-container');
    assertEquals(4, thumbnails!.length, 'should be 4 images available.');
  });

  test('selects thumbnail on click', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailsLoading = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const thumbnail =
        seaPenImagesElement.shadowRoot!.querySelector<HTMLElement>(
            'div:not([hidden]).thumbnail-item-container img');
    thumbnail!.click();

    const id = await seaPenProvider.whenCalled('selectSeaPenThumbnail');
    assertEquals(
        seaPenProvider.images[0]!.id, id, 'id sent for first SeaPenThumbnail');
  });
});

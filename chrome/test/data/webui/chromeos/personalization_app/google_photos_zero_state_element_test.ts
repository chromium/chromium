// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {GooglePhotosTab, GooglePhotosZeroStateElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {initElement} from './personalization_app_test_utils.js';

suite('GooglePhotosZeroStateElementTest', function() {
  let googlePhotosZeroStateElement: GooglePhotosZeroStateElement|null;

  test('displays no message without tab', async () => {
    googlePhotosZeroStateElement = initElement(GooglePhotosZeroStateElement);
    await waitAfterNextRender(googlePhotosZeroStateElement);

    assertEquals(
        null,
        googlePhotosZeroStateElement.shadowRoot!.getElementById('message'),
        'no message shown');
  });

  test('displays correct message for albums and photos tab', async () => {
    googlePhotosZeroStateElement = initElement(GooglePhotosZeroStateElement);
    for (const tab of [GooglePhotosTab.ALBUMS, GooglePhotosTab.PHOTOS]) {
      googlePhotosZeroStateElement.tab = tab;
      await waitAfterNextRender(googlePhotosZeroStateElement);

      const localizedLink =
          googlePhotosZeroStateElement.shadowRoot!.querySelector(
              'localized-link');

      assertTrue(!!localizedLink, 'localized link exists');

      // `localizedLink.localizedString` typescript type is string but is
      // actually TrustedHTML.
      assertTrue(
          (localizedLink.localizedString as unknown) instanceof TrustedHTML,
          'localizedLink has message set as TrustedHTML');
      assertEquals(
          'No image available. To add photos, go to ' +
              '<a target="_blank" href="https://photos.google.com">' +
              'photos.google.com</a>',
          localizedLink.localizedString.toString(),
          'localized link message matches');
    }
  });

  test('displays correct message for photos by album id tab', async () => {
    googlePhotosZeroStateElement = initElement(GooglePhotosZeroStateElement);
    googlePhotosZeroStateElement.tab = GooglePhotosTab.PHOTOS_BY_ALBUM_ID;
    await waitAfterNextRender(googlePhotosZeroStateElement);

    const localizedLink =
        googlePhotosZeroStateElement.shadowRoot!.querySelector(
            'localized-link');

    assertTrue(!!localizedLink, 'localized link exists');

    // `localizedLink.localizedString` typescript type is string but is
    // actually TrustedHTML.
    assertTrue(
        (localizedLink.localizedString as unknown) instanceof TrustedHTML,
        'localizedLink has message set as TrustedHTML');

    assertEquals(
        `This album doesn't have any photos. ` +
            'To add photos, go to ' +
            '<a target="_blank" href="https://photos.google.com">' +
            'photos.google.com</a>',
        localizedLink.localizedString.toString(),
        'inner text matches on photos_by_album_id tab');
  });
});

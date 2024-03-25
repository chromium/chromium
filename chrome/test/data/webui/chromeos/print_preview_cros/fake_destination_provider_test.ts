// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/fakes/fake_destination_provider.js';

import {FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY, FakeDestinationProvider, GET_LOCAL_DESTINATIONS_METHOD} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('FakeDestinationProvider', () => {
  let destinationProvider: FakeDestinationProvider;

  setup(() => {
    destinationProvider = new FakeDestinationProvider();
  });

  // Verify initial call count for tracked methods is zero.
  test('call count zero', () => {
    assertEquals(
        0, destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
        `${GET_LOCAL_DESTINATIONS_METHOD} should not have been called`);
  });

  // Verify default return value is
  // FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY.
  test(
      'getLocalPrinters returns FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY',
      async () => {
        const expectedCallCount = 1;
        const result = await destinationProvider.getLocalDestinations();

        assertEquals(
            expectedCallCount,
            destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
            `Called once`);
        assertDeepEquals(
            FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY, result,
            'Returns FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY');
      });
});

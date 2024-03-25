// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/destination_manager.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {FakeDestinationProvider, GET_LOCAL_DESTINATIONS_METHOD} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {Destination} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {assertEquals, assertFalse, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('DestinationManager', () => {
  let instance: DestinationManager;
  let destinationProvider: FakeDestinationProvider;

  setup(() => {
    DestinationManager.resetInstanceForTesting();
    destinationProvider = new FakeDestinationProvider();
    setDestinationProviderForTesting(destinationProvider);

    instance = DestinationManager.getInstance();
  });

  teardown(() => {
    DestinationManager.resetInstanceForTesting();
    destinationProvider.reset();
  });

  test('is a singleton', () => {
    const instance1 = DestinationManager.getInstance();
    const instance2 = DestinationManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = DestinationManager.getInstance();
    DestinationManager.resetInstanceForTesting();
    const instance2 = DestinationManager.getInstance();
    assertNotEquals(instance1, instance2, 'Reset clears static instance');
  });

  // Verify `hasInitialDestinationsLoaded` returns false by default.
  test('on create hasInitialDestinationsLoaded is false', () => {
    assertFalse(instance.hasInitialDestinationsLoaded());
  });

  // Verify PDF printer included in destinations.
  test('getDestinations contains PDF printer', () => {
    const destinations: Destination[] = instance.getDestinations();
    const pdfIndex =
        destinations.findIndex((d: Destination) => d.id === PDF_DESTINATION.id);
    const notFoundIndex = -1;
    assertNotEquals(notFoundIndex, pdfIndex, 'PDF destination available');
  });

  // Verify getLocalPrinters is called on construction of manager.
  test('on create getLocalPrinters is called', () => {
    const expectedCallCount = 1;
    assertEquals(
        expectedCallCount,
        destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
        `${GET_LOCAL_DESTINATIONS_METHOD} called in construction of manager`);
  });
});

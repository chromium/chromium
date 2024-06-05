// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/fakes/fake_destination_provider.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY, FakeDestinationProvider, GET_LOCAL_DESTINATIONS_METHOD, getFakeCapabilitiesResponse, OBSERVE_DESTINATION_CHANGES_METHOD} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {Destination, FakeDestinationObserverInterface} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {PrinterType} from 'chrome://os-print/print.mojom-webui.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

// Test implementation of FakeDestinationObserverInterface used to verify
// observer called with expected data and call count.
class TestDestinationObserver implements FakeDestinationObserverInterface {
  private expectedDestinations: Destination[];
  private callCount = 0;

  constructor(expectedDestinations: Destination[] = []) {
    this.expectedDestinations = expectedDestinations;
  }

  onDestinationsChanged(destinations: Destination[]): void {
    ++this.callCount;
    assertDeepEquals(
        this.expectedDestinations, destinations,
        'observer.onDestinationsChanged destinations should equal' +
            ' expectedDestinations');
  }

  getCallCount(): number {
    return this.callCount;
  }
}

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
    assertEquals(
        0, destinationProvider.getCallCount(OBSERVE_DESTINATION_CHANGES_METHOD),
        `${OBSERVE_DESTINATION_CHANGES_METHOD} should not have been called`);
  });

  // Verify default return value is
  // FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY.
  test(
      'getLocalDestinations returns successful result with no destinations',
      async () => {
        const expectedCallCount = 1;
        const result = await destinationProvider.getLocalDestinations();

        assertEquals(
            expectedCallCount,
            destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
            `Called once`);
        assertDeepEquals(
            FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY, result.destinations,
            'Returns FAKE_GET_LOCAL_DESTINATIONS_SUCCESSFUL_EMPTY');
      });

  // Verify observeDestinationChanges sets up observer and enables triggering
  // callback with provided data.
  test(
      'observeDestinationChanges sets up observer and enables triggering ' +
          'callback with provided data.',
      () => {
        const expectedDestinations: Destination[] = [PDF_DESTINATION];
        const testObserver = new TestDestinationObserver(expectedDestinations);
        let expectedCallCount = 0;
        assertEquals(
            expectedCallCount, testObserver.getCallCount(),
            `Observer not called`);

        // Configure observer to watch for destination changes.
        destinationProvider.observeDestinationChanges(testObserver);

        assertEquals(
            1,
            destinationProvider.getCallCount(
                OBSERVE_DESTINATION_CHANGES_METHOD),
            `${OBSERVE_DESTINATION_CHANGES_METHOD} called once`);
        assertEquals(
            expectedCallCount, testObserver.getCallCount(),
            `Observer not called`);

        destinationProvider.setDestinationsChangesData(expectedDestinations);
        destinationProvider.triggerOnDestinationsChanged();
        ++expectedCallCount;

        assertEquals(
            expectedCallCount, testObserver.getCallCount(), `Observer called`);

        // Observer triggered multiple times.
        destinationProvider.triggerOnDestinationsChanged();
        destinationProvider.triggerOnDestinationsChanged();
        expectedCallCount += 2;

        assertEquals(
            expectedCallCount, testObserver.getCallCount(),
            `Observer called multiple times`);
      });

  // Verify the fake DestinationProvider returns the expected capabilities
  // when fetchCapabilities() is called.
  test('call fetch capabilities', async () => {
    const capabilities = await destinationProvider.fetchCapabilities(
        /*destinationId=*/ '', PrinterType.kLocal);
    assertDeepEquals(getFakeCapabilitiesResponse(), capabilities);
  });

  // Verify the fetchCapabilities() returns the set capabilities.
  test('set capabilities', async () => {
    const expectedCapabilities =
        getFakeCapabilitiesResponse('New Destination').capabilities;
    destinationProvider.setCapabilities(expectedCapabilities);

    const capabilities = await destinationProvider.fetchCapabilities(
        /*destinationId=*/ '', PrinterType.kLocal);
    assertDeepEquals(expectedCapabilities, capabilities.capabilities);
  });
});

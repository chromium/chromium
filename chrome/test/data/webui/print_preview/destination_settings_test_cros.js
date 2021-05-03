// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, DestinationType, LocalDestinationInfo, NativeLayer, NativeLayerCros, NativeLayerCrosImpl, NativeLayerImpl, NUM_PERSISTED_DESTINATIONS, RecentDestination, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise, fakeDataBind, waitBeforeNextRender} from '../test_util.m.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

window.destination_settings_test_cros = {};
const destination_settings_test_cros = window.destination_settings_test_cros;
destination_settings_test_cros.suiteName = 'DestinationSettingsCrosTest';
/** @enum {string} */
destination_settings_test_cros.TestNames = {
  EulaIsRetrieved: 'eula is retrieved',
  DriveIsNotMounted: 'drive is not mounted',
};

suite(destination_settings_test_cros.suiteName, function() {
  /** @type {!PrintPreviewDestinationSettingsElement} */
  let destinationSettings;

  /** @type {?NativeLayerStub} */
  let nativeLayer = null;

  /** @type {?NativeLayerCrosStub} */
  let nativeLayerCros = null;

  /** @type {!Array<!LocalDestinationInfo>} */
  let localDestinations = [];

  /** @type {!Array<!Destination>} */
  let destinations = [];

  /** @type {boolean} */
  let isDriveMounted = true;

  /** @type {string} */
  const defaultUser = 'foo@chromium.org';

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    // Stub out native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.instance_ = nativeLayerCros;

    localDestinations = [];
    destinations = getDestinations(localDestinations);
    // Add some extra destinations.
    for (let i = 0; i < NUM_PERSISTED_DESTINATIONS; i++) {
      const id = `e${i}`;
      const name = `n${i}`;
      localDestinations.push({deviceName: id, printerName: name});
    }
    nativeLayer.setLocalDestinations(localDestinations);

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    destinationSettings =
        /** @type {!PrintPreviewDestinationSettingsElement} */ (
            document.createElement('print-preview-destination-settings'));
    destinationSettings.settings = model.settings;
    destinationSettings.state = State.NOT_READY;
    destinationSettings.disabled = true;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);
  });

  /**
   * Initializes the destination store and destination settings using
   * |destinations| and |recentDestinations|.
   */
  function initialize() {
    // Initialize destination settings.
    destinationSettings.setSetting('recentDestinations', []);
    destinationSettings.appKioskMode = false;
    destinationSettings.init(
        '' /* printerName */, false, isDriveMounted,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
  }

  /**
   * @param {!Array<string>} expectedDestinationIds An array of the expected
   *     recent destination ids.
   */
  function assertRecentDestinations(expectedDestinationIds) {
    const recentDestinations =
        destinationSettings.getSettingValue('recentDestinations');
    assertEquals(expectedDestinationIds.length, recentDestinations.length);
    expectedDestinationIds.forEach((expectedId, index) => {
      assertEquals(expectedId, recentDestinations[index].id);
    });
  }

  function selectDestination(destination) {
    const storeDestination =
        destinationSettings.getDestinationStoreForTest().destinations().find(
            d => d.key === destination.key);
    destinationSettings.getDestinationStoreForTest().selectDestination(
        assert(storeDestination));
    flush();
  }


  /**
   * Tests that destinations with a EULA will fetch the EULA URL when
   * selected.
   */
  test(
      assert(destination_settings_test_cros.TestNames.EulaIsRetrieved),
      function() {
        // Recent destinations start out empty.
        assertRecentDestinations([]);

        const expectedUrl = 'chrome://os-credits/eula';

        assertEquals(0, nativeLayerCros.getCallCount('getEulaUrl'));

        initialize();

        return nativeLayerCros.whenCalled('getEulaUrl')
            .then(() => {
              assertEquals(1, nativeLayerCros.getCallCount('getEulaUrl'));
              nativeLayerCros.resetResolver('getEulaUrl');

              // Add printers to the store.
              destinationSettings.getDestinationStoreForTest()
                  .startLoadAllDestinations();
              return nativeLayer.whenCalled('getPrinters');
            })
            .then(() => {
              nativeLayerCros.setEulaUrl('chrome://os-credits/eula');
              // Simulate selecting a destination that has a EULA URL from the
              // dialog.
              selectDestination(destinations[0]);
              return nativeLayerCros.whenCalled('getEulaUrl');
            })
            .then(() => {
              assertEquals(1, nativeLayerCros.getCallCount('getEulaUrl'));
              nativeLayerCros.resetResolver('getEulaUrl');
              assertEquals(
                  expectedUrl, destinationSettings.destination.eulaUrl);

              nativeLayerCros.setEulaUrl('');
              // Select a destination without a EULA URL.
              selectDestination(destinations[1]);
              return nativeLayerCros.whenCalled('getEulaUrl');
            })
            .then(() => {
              assertEquals(1, nativeLayerCros.getCallCount('getEulaUrl'));
              nativeLayerCros.resetResolver('getEulaUrl');
              assertEquals('', destinationSettings.destination.eulaUrl);

              // Reselect a destination with a EULA URL. This destination
              // already had its EULA URL set, so expect that it still retains
              // it. Since capabilities for this destination are already set,
              // we don't try to fetch the license again.
              nativeLayer.resetResolver('getPrinterCapabilities');
              destinationSettings.$$('#destinationSelect')
                  .fire('selected-option-change', 'ID1/chrome_os/');
            })
            .then(() => {
              assertEquals(
                  0, nativeLayer.getCallCount('getPrinterCapabilities'));
              assertEquals(0, nativeLayerCros.getCallCount('getEulaUrl'));
              assertRecentDestinations(['ID1', 'ID2', 'Save as PDF']);
              assertEquals(
                  expectedUrl, destinationSettings.destination.eulaUrl);
            });
      });

  // Tests that disabling Google Drive on Chrome OS hides the Save to Drive
  // destination.
  test(
      assert(destination_settings_test_cros.TestNames.DriveIsNotMounted),
      function() {
        isDriveMounted = false;
        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              const options = destinationSettings.$$('#destinationSelect')
                                  .getVisibleItemsForTest();
              assertEquals(2, options.length);
              assertEquals('Save as PDF/local/', options[0].value);
            });
      });
});

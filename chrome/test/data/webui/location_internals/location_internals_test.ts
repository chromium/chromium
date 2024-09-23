// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LAST_NETWORK_REQUEST_TABLE_ID, LAST_NETWORK_RESPONSE_TABLE_ID, POSITION_CACHE_TABLE_ID, WIFI_DATA_TABLE_ID, WIFI_POLLING_POLICY_TABLE_ID} from 'chrome://location-internals/diagnose_info_view.js';
import type {AccessPointData, GeolocationDiagnostics, GeolocationInternalsInterface, GeolocationInternalsObserverRemote, GeolocationInternalsPendingReceiver, NetworkLocationResponse} from 'chrome://location-internals/geolocation_internals.mojom-webui.js';
import {GeolocationInternalsReceiver, INVALID_CHANNEL, INVALID_RADIO_SIGNAL_STRENGTH, INVALID_SIGNAL_TO_NOISE} from 'chrome://location-internals/geolocation_internals.mojom-webui.js';
import {BAD_ACCURACY, BAD_ALTITUDE, BAD_HEADING, BAD_LATITUDE_LONGITUDE, BAD_SPEED} from 'chrome://location-internals/geoposition.mojom-webui.js';
import {DIAGNOSE_INFO_VIEW_ID, initializeMojo, REFRESH_FINISH_EVENT, REFRESH_STATUS_ID, REFRESH_STATUS_SUCCESS, REFRESH_STATUS_UNINITIALIZED, WATCH_BUTTON_ID} from 'chrome://location-internals/location_internals.js';
import type {LocationInternalsHandlerInterface} from 'chrome://location-internals/location_internals.mojom-webui.js';
import {LocationInternalsHandler, LocationInternalsHandlerReceiver} from 'chrome://location-internals/location_internals.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {Time, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

let geolocationInternalsRemote: FakeGeolocationInternalsRemote|null = null;

// Updates diagnostic information, then clicks the refresh button and returns a
// promise that resolves when the display has updated.
function simulateDiagnosticsUpdate(diagnostics: GeolocationDiagnostics) {
  const promise = eventToPromise(REFRESH_FINISH_EVENT, window);
  geolocationInternalsRemote!.installDiagnostics(diagnostics);
  return promise;
}

function simulateNetworkLocationRequest(request: AccessPointData[]) {
  const promise = eventToPromise(REFRESH_FINISH_EVENT, window);
  geolocationInternalsRemote!.simulateNetworkLocationRequest(request);
  return promise;
}

function simulateNetworkLocationResponse(response: NetworkLocationResponse|
                                         null) {
  const promise = eventToPromise(REFRESH_FINISH_EVENT, window);
  geolocationInternalsRemote!.simulateNetworkLocationResponse(response);
  return promise;
}

// Converts `date` from Javascript `Date` to `mojom_base.mojom.Time`.
function dateToMojoTime(date: Date) {
  // The Javascript `Date()` is based off of the number of milliseconds since
  // the UNIX epoch (1970-01-01 00::00:00 UTC), while `internalValue``
  // of the `base::Time` (represented in mojom.Time) represents the
  // number of microseconds since the Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC). This computes the final `Date` by
  // computing the epoch delta and the conversion from microseconds to
  // milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // `epochDeltaInMs` is equal to `base::Time::kTimeTToMicrosecondsOffset`.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const internalValue = BigInt(date.valueOf() + epochDeltaInMs) * BigInt(1000);
  return {internalValue} as Time;
}

/**
 * Converts a time delta in milliseconds to TimeDelta.
 * @param milliseconds time delta in milliseconds
 */
function millisecondsToMojoTimeDelta(milliseconds: number): TimeDelta {
  return {microseconds: BigInt(Math.floor(milliseconds * 1000))};
}

// Checks that the table with ID `tableId` is not shown.
function checkTableHidden(tableId: string) {
  const diagnoseInfoView =
      getRequiredElement<HTMLElement>(DIAGNOSE_INFO_VIEW_ID);
  assert(diagnoseInfoView);
  const diagnoseInfoTable =
      diagnoseInfoView.shadowRoot!.querySelector<HTMLElement>(`#${tableId}`);
  assert(diagnoseInfoTable);
  assert(diagnoseInfoTable.style.display === 'none');
}

// Checks that the table with ID `tableId` has the specified `title`,
// `headers`, and `rows`. If `footerPrefix` is provided, it also checks that the
// table footer starts with the specified prefix. Otherwise, it checks that the
// footer is empty.
function checkTableContents(
    tableId: string, title: string, headers: string[], rows: string[][],
    footerPrefix: string|undefined = undefined) {
  const diagnoseInfoView =
      getRequiredElement<HTMLElement>(DIAGNOSE_INFO_VIEW_ID);
  assert(diagnoseInfoView);
  const diagnoseInfoTable =
      diagnoseInfoView.shadowRoot!.querySelector<HTMLElement>(`#${tableId}`);
  assert(diagnoseInfoTable);
  assert(diagnoseInfoTable.style.display !== 'none');
  const tableElement = diagnoseInfoTable.shadowRoot!.querySelector('table');
  assert(tableElement);
  const titleElement = tableElement.querySelector('#table-title');
  assert(titleElement);
  assert(titleElement.textContent === title);
  const theadElement = tableElement.querySelector('thead');
  assert(theadElement);
  const trElement = theadElement.querySelector('tr');
  assert(trElement);
  const thElements = trElement.querySelectorAll('th');
  assert(thElements);
  assert(thElements.length === headers.length);
  for (let columnIndex = 0; columnIndex < headers.length; ++columnIndex) {
    assert(thElements[columnIndex]!.textContent === headers[columnIndex]);
  }
  const tbodyElement = tableElement.querySelector('tbody');
  assert(tbodyElement);
  const trElements = tbodyElement.querySelectorAll('tr');
  assert(trElements);
  assert(trElements.length === rows.length);
  for (let rowIndex = 0; rowIndex < rows.length; ++rowIndex) {
    const row = rows[rowIndex]!;
    assert(trElements[rowIndex]);
    const tdElements = trElements[rowIndex]!.querySelectorAll('td');
    assert(tdElements);
    assert(tdElements.length === row.length);
    for (let columnIndex = 0; columnIndex < row.length; ++columnIndex) {
      assert(tdElements[columnIndex]!.textContent === row[columnIndex]);
    }
  }
  const footerElement = tableElement.querySelector('#table-footer');
  assert(footerElement);
  if (footerPrefix === undefined) {
    assert(footerElement!.textContent === '');
  } else {
    assert(footerElement!.textContent!.startsWith(footerPrefix));
  }
}

class FakeLocationInternalsHandlerRemote extends TestBrowserProxy implements
    LocationInternalsHandlerInterface {
  private receiver_: LocationInternalsHandlerReceiver;
  geolocationInternalsRemote: FakeGeolocationInternalsRemote|null = null;

  constructor(handle: MojoHandle) {
    super([
      'bindInternalsInterface',
    ]);
    this.receiver_ = new LocationInternalsHandlerReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  bindInternalsInterface(geolocationInternalPendingReceiver:
                             GeolocationInternalsPendingReceiver) {
    this.methodCalled(
        'bindInternalsInterface', geolocationInternalPendingReceiver);
    geolocationInternalsRemote =
        new FakeGeolocationInternalsRemote(geolocationInternalPendingReceiver);
  }
}

class FakeGeolocationInternalsRemote extends TestBrowserProxy implements
    GeolocationInternalsInterface {
  private receiver_: GeolocationInternalsReceiver;
  private observer_: GeolocationInternalsObserverRemote|null;
  private diagnostics_: GeolocationDiagnostics|null;

  constructor(pendingReceiver: GeolocationInternalsPendingReceiver) {
    super([
      'getDiagnostics',
    ]);

    this.receiver_ = new GeolocationInternalsReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    this.observer_ = null;
    this.diagnostics_ = null;
  }

  addInternalsObserver(observer: GeolocationInternalsObserverRemote):
      Promise<{diagnostics: (GeolocationDiagnostics | null)}> {
    this.observer_ = observer;
    return Promise.resolve({diagnostics: this.diagnostics_});
  }

  installDiagnostics(diagnostics: GeolocationDiagnostics) {
    this.diagnostics_ = diagnostics;
    if (this.observer_ !== null) {
      this.observer_.onDiagnosticsChanged(this.diagnostics_);
    }
  }

  simulateNetworkLocationRequest(request: AccessPointData[]) {
    if (this.observer_ !== null) {
      this.observer_.onNetworkLocationRequested(request);
    }
  }

  simulateNetworkLocationResponse(request: NetworkLocationResponse|null) {
    if (this.observer_ !== null) {
      this.observer_.onNetworkLocationReceived(request);
    }
  }
}

suite('LocationInternalsUITest', function() {
  let fakeLocationInternalsHandler: FakeLocationInternalsHandlerRemote|null =
      null;

  suiteSetup(async function() {
    const internalsHandlerInterceptor =
        new MojoInterfaceInterceptor(LocationInternalsHandler.$interfaceName);
    internalsHandlerInterceptor.oninterfacerequest = (e) => {
      fakeLocationInternalsHandler =
          new FakeLocationInternalsHandlerRemote(e.handle);
    };
    internalsHandlerInterceptor.start();
    initializeMojo();
  });

  teardown(function() {
    fakeLocationInternalsHandler?.reset();
    geolocationInternalsRemote?.reset();
  });

  test('PageLoaded', async function() {
    const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
    assert(watchButton);
  });

  test('RefreshStatus', async function() {
    // Check that the initial status indicates the API is not initialized.
    const refreshStatus = getRequiredElement<HTMLElement>(REFRESH_STATUS_ID);
    assert(refreshStatus.textContent!.includes(REFRESH_STATUS_UNINITIALIZED));

    // Simulate an update and check that the status message indicates success.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    assert(refreshStatus.textContent!.includes(REFRESH_STATUS_SUCCESS));
  });

  test('NetworkLocationDiagnosticsHidden', async function() {
    // Simulate geolocation not yet initialized.
    checkTableHidden(WIFI_DATA_TABLE_ID);

    // Simulate network location provider not initialized.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableHidden(WIFI_DATA_TABLE_ID);
  });

  test('NetworkLocationDiagnosticsEmptyWifiData', async function() {
    // Simulate network provider created but no data received yet.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      networkLocationDiagnostics: {accessPointData: [], wifiTimestamp: null},
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableContents(
        WIFI_DATA_TABLE_ID, WIFI_DATA_TABLE_ID,
        [
          'MAC address',
          'Signal strength',
          'Channel',
          'Signal to Noise Ratio',
          'Timestamp',
        ],
        [['No access point data', '', '', '', '']], 'No Wi-Fi data received');
  });

  test('NetworkLocationDiagnosticsGotWifiData', async function() {
    // Simulate network provider receiving data for two access points.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      networkLocationDiagnostics: {
        accessPointData: [
          {
            macAddress: '00-11-22-33-44-55',
            radioSignalStrength: -50,
            channel: 1,
            signalToNoise: 10,
            timestamp: dateToMojoTime(new Date('2020-01-12T22:25:00')),
          },
          {
            macAddress: 'aa-bb-cc-dd-ee-ff',
            radioSignalStrength: -42,
            channel: 2,
            signalToNoise: 15,
            timestamp: dateToMojoTime(new Date('2020-01-12T22:26:00')),
          },
        ],
        wifiTimestamp: dateToMojoTime(new Date('2020-01-12T22:27:00')),
      },
      wifiPollingPolicyDiagnostics: null,
      positionCacheDiagnostics: null,
    });
    checkTableContents(
        WIFI_DATA_TABLE_ID, WIFI_DATA_TABLE_ID,
        [
          'MAC address',
          'Signal strength',
          'Channel',
          'Signal to Noise Ratio',
          'Timestamp',
        ],
        [
          [
            '00-11-22-33-44-55',
            '-50 dBm',
            '1',
            '10 dB',
            '1/12/2020, 10:25:00 PM',
          ],
          [
            'aa-bb-cc-dd-ee-ff',
            '-42 dBm',
            '2',
            '15 dB',
            '1/12/2020, 10:26:00 PM',
          ],
        ],
        'Wi-Fi data last received 1/12/2020, 10:27:00 PM');
  });

  test('NetworkLocationDiagnosticsInvalidWifiData', async function() {
    // Simulate network provider receiving invalid access point data.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      networkLocationDiagnostics: {
        accessPointData: [{
          macAddress: '00-11-22-33-44-55',
          radioSignalStrength: INVALID_RADIO_SIGNAL_STRENGTH,
          channel: INVALID_CHANNEL,
          signalToNoise: INVALID_SIGNAL_TO_NOISE,
          timestamp: null,
        }],
        wifiTimestamp: dateToMojoTime(new Date('2020-01-12T22:27:00')),
      },
      wifiPollingPolicyDiagnostics: null,
      positionCacheDiagnostics: null,
    });
    checkTableContents(
        WIFI_DATA_TABLE_ID, WIFI_DATA_TABLE_ID,
        [
          'MAC address',
          'Signal strength',
          'Channel',
          'Signal to Noise Ratio',
          'Timestamp',
        ],
        [['00-11-22-33-44-55', 'N/A', 'N/A', 'N/A', 'N/A']],
        'Wi-Fi data last received 1/12/2020, 10:27:00 PM');
  });

  test('PositionCacheDiagnosticsHidden', async function() {
    // Simulate geolocation not yet initialized.
    checkTableHidden(POSITION_CACHE_TABLE_ID);

    // Simulate uninitialized position cache.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableHidden(POSITION_CACHE_TABLE_ID);
  });

  test('PositionCacheDiagnosticsEmpty', async function() {
    // Simulate position cache created but no cached data yet.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      positionCacheDiagnostics: {
        cacheSize: 0,
        hitRate: null,
        lastMiss: null,
        lastHit: null,
        lastNetworkResult: null,
      },
      networkLocationDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableContents(
        POSITION_CACHE_TABLE_ID, POSITION_CACHE_TABLE_ID,
        [
          'Cache size',
          'Last cache hit',
          'Last cache miss',
          'Cache hit rate',
          'Last result',
        ],
        [['0', 'None', 'None', 'N/A', 'None']]);
  });

  test('PositionCacheDiagnosticsPopulated', async function() {
    // Simulate a populated position cache with `lastNetworkResult` set to a
    // `Geoposition`.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: {
        cacheSize: 1,
        lastHit: dateToMojoTime(new Date('2020-01-12T22:27:00')),
        lastMiss: dateToMojoTime(new Date('2020-01-12T22:14:00')),
        hitRate: 0.5,
        lastNetworkResult: {
          position: {
            latitude: 37.0,
            longitude: -112.0,
            altitude: 32.0,
            accuracy: 5.0,
            altitudeAccuracy: 10.0,
            heading: 90.0,
            speed: 1.0,
            timestamp: dateToMojoTime(new Date('2020-01-12T22:27:00')),
          },
        },
      },
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableContents(
        POSITION_CACHE_TABLE_ID, POSITION_CACHE_TABLE_ID,
        [
          'Cache size',
          'Last cache hit',
          'Last cache miss',
          'Cache hit rate',
          'Last result',
        ],
        [[
          '1',
          '1/12/2020, 10:27:00 PM',
          '1/12/2020, 10:14:00 PM',
          '50%',
          '37°, -112° ±5 m; 32 m ±10 m; 90°; 1 m/s; 1/12/2020, 10:27:00 PM',
        ]]);
  });

  test('PositionCacheDiagnosticsBadValues', async function() {
    // Set `lastNetworkResult` to a `Geoposition` with sentinel values to mark
    // invalid data.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      positionCacheDiagnostics: {
        cacheSize: 0,
        hitRate: null,
        lastHit: null,
        lastMiss: null,
        lastNetworkResult: {
          position: {
            latitude: BAD_LATITUDE_LONGITUDE,
            longitude: BAD_LATITUDE_LONGITUDE,
            altitude: BAD_ALTITUDE,
            accuracy: BAD_ACCURACY,
            altitudeAccuracy: BAD_ACCURACY,
            heading: BAD_HEADING,
            speed: BAD_SPEED,
            timestamp: dateToMojoTime(new Date('2020-01-12T22:27:00')),
          },
        },
      },
      networkLocationDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,

    });
    checkTableContents(
        POSITION_CACHE_TABLE_ID, POSITION_CACHE_TABLE_ID,
        [
          'Cache size',
          'Last cache hit',
          'Last cache miss',
          'Cache hit rate',
          'Last result',
        ],
        [[
          '0',
          'None',
          'None',
          'N/A',
          'Invalid geoposition',
        ]]);

    // Check that the position is displayed if `latitude` and `longitude` are
    // both valid.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      positionCacheDiagnostics: {
        cacheSize: 0,
        lastNetworkResult: {
          position: {
            latitude: 37.0,
            longitude: -112.0,
            altitude: BAD_ALTITUDE,
            accuracy: BAD_ACCURACY,
            altitudeAccuracy: BAD_ACCURACY,
            heading: BAD_HEADING,
            speed: BAD_SPEED,
            timestamp: dateToMojoTime(new Date('2020-01-12T22:27:00')),
          },
        },
        hitRate: null,
        lastHit: null,
        lastMiss: null,
      },
      networkLocationDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableContents(
        POSITION_CACHE_TABLE_ID, POSITION_CACHE_TABLE_ID,
        [
          'Cache size',
          'Last cache hit',
          'Last cache miss',
          'Cache hit rate',
          'Last result',
        ],
        [[
          '0',
          'None',
          'None',
          'N/A',
          '37°, -112°; 1/12/2020, 10:27:00 PM',
        ]]);
  });

  test('PositionCacheDiagnosticsGeopositionError', async function() {
    // Set `lastNetworkResult` to a `GeopositionError`.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      positionCacheDiagnostics: {
        cacheSize: 0,
        lastNetworkResult: {
          error: {
            errorCode: 1,
            errorMessage: 'User denied Geolocation',
            errorTechnical: 'error-technical',
          },
        },
        hitRate: null,
        lastHit: null,
        lastMiss: null,
      },
      networkLocationDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableContents(
        POSITION_CACHE_TABLE_ID, POSITION_CACHE_TABLE_ID,
        [
          'Cache size',
          'Last cache hit',
          'Last cache miss',
          'Cache hit rate',
          'Last result',
        ],
        [[
          '0',
          'None',
          'None',
          'N/A',
          'User denied Geolocation (1)',
        ]]);
  });

  test('WifiPollingPolicyTableHidden', async function() {
    // Geolocation not yet initialized.
    checkTableHidden(WIFI_POLLING_POLICY_TABLE_ID);

    // Simulate wifi polling policy not initialized.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableHidden(WIFI_POLLING_POLICY_TABLE_ID);
  });

  test('WifiPollingPolicyTablePopulated', async function() {
    // Simulate valid wifi polling policy data is populated.
    await simulateDiagnosticsUpdate({
      providerState: 1,
      positionCacheDiagnostics: null,
      networkLocationDiagnostics: null,
      wifiPollingPolicyDiagnostics: {
        intervalStart: dateToMojoTime(new Date('2020-01-12T22:27:00')),
        intervalDuration:
            millisecondsToMojoTimeDelta(2 * 60 * 1000),  // 2 minutes
        pollingInterval:
            millisecondsToMojoTimeDelta(2 * 60 * 1000),           // 2 minutes
        defaultInterval: millisecondsToMojoTimeDelta(10 * 1000),  // 10 seconds
        noChangeInterval:
            millisecondsToMojoTimeDelta(2 * 60 * 1000),  // 2 minutes
        twoNoChangeInterval:
            millisecondsToMojoTimeDelta(10 * 60 * 1000),         // 10 minutes
        noWifiInterval: millisecondsToMojoTimeDelta(20 * 1000),  // 20 seconds
      },
    });
    checkTableContents(
        WIFI_POLLING_POLICY_TABLE_ID, WIFI_POLLING_POLICY_TABLE_ID,
        [
          'Interval start time',
          'Interval duration (sec)',
          'Polling interval (sec)',
          'Default interval (sec)',
          'No change interval (sec)',
          'Two no change interval (sec)',
          'No Wi-Fi interval (sec)',
        ],
        [[
          '1/12/2020, 10:27:00 PM',
          '120',
          '120',
          '10',
          '120',
          '600',
          '20',
        ]]);
  });

  test('NetworkLocationRequestHidden', async function() {
    // The network request table remains hidden until the first request is
    // created.
    checkTableHidden(LAST_NETWORK_REQUEST_TABLE_ID);

    // Updating diagnostics does not display the network request table.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableHidden(LAST_NETWORK_REQUEST_TABLE_ID);
  });

  test('NetworkLocationRequestEmpty', async function() {
    // Simulate an empty update. The table is displayed with a message
    // indicating no access points were sent.
    await simulateNetworkLocationRequest([]);
    checkTableContents(
        LAST_NETWORK_REQUEST_TABLE_ID, LAST_NETWORK_REQUEST_TABLE_ID,
        [
          'MAC address',
          'Signal strength',
          'Channel',
          'Signal to Noise Ratio',
          'Timestamp',
        ],
        [[
          'No access point data',
          '',
          '',
          '',
          '',
        ]],
        'Request sent at ');
  });

  test('NetworkLocationRequestPopulated', async function() {
    // Simulate an update with one access point.
    await simulateNetworkLocationRequest([{
      macAddress: 'aa-bb-cc-dd-ee-ff',
      radioSignalStrength: -42,
      channel: 2,
      signalToNoise: 15,
      timestamp: dateToMojoTime(new Date('2020-01-12T22:26:00')),
    }]);
    checkTableContents(
        LAST_NETWORK_REQUEST_TABLE_ID, LAST_NETWORK_REQUEST_TABLE_ID,
        [
          'MAC address',
          'Signal strength',
          'Channel',
          'Signal to Noise Ratio',
          'Timestamp',
        ],
        [[
          'aa-bb-cc-dd-ee-ff',
          '-42 dBm',
          '2',
          '15 dB',
          '1/12/2020, 10:26:00 PM',
        ]],
        'Request sent at ');
  });

  test('NetworkLocationResponseHidden', async function() {
    // The network response table remains hidden until the first response is
    // received.
    checkTableHidden(LAST_NETWORK_RESPONSE_TABLE_ID);

    // Updating diagnostics does not display the network response table.
    await simulateDiagnosticsUpdate({
      providerState: 0,
      networkLocationDiagnostics: null,
      positionCacheDiagnostics: null,
      wifiPollingPolicyDiagnostics: null,
    });
    checkTableHidden(LAST_NETWORK_RESPONSE_TABLE_ID);
  });

  test('NetworkLocationResponseInvalid', async function() {
    // Set the response to null to simulate an invalid response.
    await simulateNetworkLocationResponse(null);
    checkTableContents(
        LAST_NETWORK_RESPONSE_TABLE_ID, LAST_NETWORK_RESPONSE_TABLE_ID,
        [
          'Position estimate',
        ],
        [[
          'None',
        ]],
        'Response received at ');
  });

  test('NetworkLocationResponsePopulated', async function() {
    // Simulate an update with all fields populated.
    await simulateNetworkLocationResponse({
      latitude: 37.0,
      longitude: -112.0,
      accuracy: 5.0,
    });
    checkTableContents(
        LAST_NETWORK_RESPONSE_TABLE_ID, LAST_NETWORK_RESPONSE_TABLE_ID,
        [
          'Position estimate',
        ],
        [[
          '37°, -112° ±5 m',
        ]],
        'Response received at ');
  });

  test('NetworkLocationResponseNoAccuracy', async function() {
    // Simulate an update without the optional accuracy field.
    await simulateNetworkLocationResponse({
      latitude: 37.0,
      longitude: -112.0,
      accuracy: null,
    });
    checkTableContents(
        LAST_NETWORK_RESPONSE_TABLE_ID, LAST_NETWORK_RESPONSE_TABLE_ID,
        [
          'Position estimate',
        ],
        [[
          '37°, -112°',
        ]],
        'Response received at ');
  });
});

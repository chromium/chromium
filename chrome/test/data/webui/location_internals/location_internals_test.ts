// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {GeolocationDiagnostics, GeolocationInternalsInterface, GeolocationInternalsPendingReceiver, GeolocationInternalsReceiver} from 'chrome://location-internals/geolocation_internals.mojom-webui.js';
import {initializeMojo, REFRESH_BUTTON_ID, REFRESH_FINISH_EVENT, REFRESH_STATUS_FAILURE, REFRESH_STATUS_ID, REFRESH_STATUS_SUCCESS, WATCH_BUTTON_ID} from 'chrome://location-internals/location_internals.js';
import {LocationInternalsHandler, LocationInternalsHandlerInterface, LocationInternalsHandlerReceiver} from 'chrome://location-internals/location_internals.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';


let geolocationInternalsRemote: FakeGeolocationInternalsRemote|null = null;

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
  private diagnostics_: GeolocationDiagnostics|null;

  constructor(pendingReceiver: GeolocationInternalsPendingReceiver) {
    super([
      'getDiagnostics',
    ]);

    this.receiver_ = new GeolocationInternalsReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    this.diagnostics_ = null;
  }

  getDiagnostics(): Promise<{diagnostics: GeolocationDiagnostics | null}> {
    this.methodCalled('getDiagnostics');
    return Promise.resolve({diagnostics: this.diagnostics_});
  }

  installDiagnostics(diagnostics: GeolocationDiagnostics|null) {
    this.diagnostics_ = diagnostics;
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

  test('RefreshButton', async function() {
    const refreshButton = getRequiredElement<HTMLElement>(REFRESH_BUTTON_ID);
    const refreshStatus = getRequiredElement<HTMLElement>(REFRESH_STATUS_ID);

    // By default `FakeGeolocationInternalsRemote` has null `data_`.
    const refreshFinishPromise1 =
        eventToPromise(REFRESH_FINISH_EVENT, refreshButton);
    refreshButton.click();
    await refreshFinishPromise1;
    assert(refreshStatus.textContent! === REFRESH_STATUS_FAILURE);

    // Installed valid data and trigger click again. On real UI we will append
    // timestamp to `refreshStatus`, here we simply validate that the
    // refreshStatus's text includes REFRESH_STATUS_SUCCESS.
    const refreshFinishPromise2 =
        eventToPromise(REFRESH_FINISH_EVENT, refreshButton);
    geolocationInternalsRemote?.installDiagnostics({providerState: 0});
    refreshButton.click();
    await refreshFinishPromise2;
    assert(refreshStatus.textContent!.includes(REFRESH_STATUS_SUCCESS));
  });
});

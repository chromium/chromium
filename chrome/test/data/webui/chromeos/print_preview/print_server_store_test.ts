// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintServerStore, PrintServerStoreEventType} from 'chrome://print/print_preview.js';
import type {WebUiListener} from 'chrome://resources/js/cr.js';
import {addWebUiListener, removeWebUiListener, webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import type {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';

suite('PrintServerStoreTest', function() {
  let printServerStore: PrintServerStore;

  let nativeLayerCros: NativeLayerCrosStub;

  let listeners: WebUiListener[];

  function addListener(eventName: string, callback: (p: any) => void) {
    listeners.push(addWebUiListener(eventName, callback));
  }

  setup(function() {
    listeners = [];
    nativeLayerCros = setNativeLayerCrosInstance();
    printServerStore = new PrintServerStore(addListener);
  });

  teardown(function() {
    listeners.forEach(removeWebUiListener);
  });

  // Tests that print servers with the selected name are selected by ID is the
  // native layer choosePrintServers is called.
  test(
      'ChoosePrintServers', async () => {
        const printServers = [
          {id: 'user-server1', name: 'Print Server 1'},
          {id: 'device-server2', name: 'Print Server 2'},
          {id: 'user-server2', name: 'Print Server 2'},
          {id: 'user-server4', name: 'Print Server 3'},
        ];

        const printServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true,
        };
        webUIListenerCallback(
            'print-servers-config-changed', printServersConfig);

        const pendingPrintServerIds =
            nativeLayerCros.whenCalled('choosePrintServers');
        printServerStore.choosePrintServers('Print Server 2');
        assertDeepEquals(
            ['device-server2', 'user-server2'], await pendingPrintServerIds);
      });

  // Tests that print servers and fetching mode are updated when
  // PRINT_SERVERS_CHANGED occurs.
  test(
      'PrintServersChanged', async () => {
        const printServers = [
          {id: 'server1', name: 'Print Server 1'},
          {id: 'server2', name: 'Print Server 2'},
        ];
        const whenPrintServersChangedEvent = eventToPromise(
            PrintServerStoreEventType.PRINT_SERVERS_CHANGED, printServerStore);

        const printServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true,
        };
        webUIListenerCallback(
            'print-servers-config-changed', printServersConfig);

        const printServersChangedEvent = await whenPrintServersChangedEvent;
        assertDeepEquals(
            ['Print Server 1', 'Print Server 2'],
            printServersChangedEvent.detail.printServerNames);
        assertTrue(printServersChangedEvent.detail.isSingleServerFetchingMode);
      });

  // Tests that print servers and fetching mode are updated when
  // getPrintServersConfig is called and an update to the print servers config
  // occurs.
  test(
      'GetPrintServersConfig', async () => {
        const printServers = [
          {id: 'server1', name: 'Print Server 1'},
          {id: 'server2', name: 'Print Server 2'},
        ];

        const expectedPrintServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true,
        };
        nativeLayerCros.setPrintServersConfig(expectedPrintServersConfig);
        const actualPrintServersConfig =
            await printServerStore.getPrintServersConfig();

        const pendingPrintServerIds =
            nativeLayerCros.whenCalled('choosePrintServers');
        printServerStore.choosePrintServers('Print Server 1');
        assertDeepEquals(['server1'], await pendingPrintServerIds);
        assertDeepEquals(printServers, actualPrintServersConfig.printServers);
        assertTrue(actualPrintServersConfig.isSingleServerFetchingMode);
      });

  // Tests that an event is dispatched are updated when SERVER_PRINTERS_LOADING
  // is called.
  test('ServerPrintersLoading', async () => {
    const whenServerPrintersLoadedEvent = eventToPromise(
        PrintServerStoreEventType.SERVER_PRINTERS_LOADING, printServerStore);

    webUIListenerCallback('server-printers-loading', true);

    const serverPrintersLoadedEvent = await whenServerPrintersLoadedEvent;
    assertTrue(serverPrintersLoadedEvent.detail);
  });
});

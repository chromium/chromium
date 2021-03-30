// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrinterType, PrintServerStore} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, WebUIListener, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

import {assertDeepEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

import {NativeLayerCrosStub, setNativeLayerCrosInstance} from './native_layer_cros_stub.js';

window.print_server_store_test = {};
const print_server_store_test = window.print_server_store_test;
print_server_store_test.suiteName = 'PrintServerStoreTest';
/** @enum {string} */
print_server_store_test.TestNames = {
  PrintServersChanged: 'print servers changed',
  GetPrintServersConfig: 'get print servers config',
  ServerPrintersLoading: 'server printers loading',
  ChoosePrintServers: 'choose print servers',
};

suite(print_server_store_test.suiteName, function() {
  /** @type {!PrintServerStore} */
  let printServerStore;

  /** @type {!NativeLayerCrosStub} */
  let nativeLayerCros;

  /** @type {!Array<WebUIListener>} */
  let listeners;

  const addListener = function() {
    listeners.push(addWebUIListener(...arguments));
  };

  /** @override */
  setup(function() {
    listeners = [];
    nativeLayerCros = setNativeLayerCrosInstance();
    printServerStore = new PrintServerStore(addListener);
  });

  /** @override */
  teardown(function() {
    listeners.forEach(removeWebUIListener);
  });

  // Tests that print servers with the selected name are selected by ID is the
  // native layer choosePrintServers is called.
  test(
      assert(print_server_store_test.TestNames.ChoosePrintServers),
      async () => {
        const printServers = [
          {id: 'user-server1', name: 'Print Server 1'},
          {id: 'device-server2', name: 'Print Server 2'},
          {id: 'user-server2', name: 'Print Server 2'},
          {id: 'user-server4', name: 'Print Server 3'},
        ];

        const printServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true
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
      assert(print_server_store_test.TestNames.PrintServersChanged),
      async () => {
        const printServers = [
          {id: 'server1', name: 'Print Server 1'},
          {id: 'server2', name: 'Print Server 2'},
        ];
        const whenPrintServersChangedEvent = eventToPromise(
            PrintServerStore.EventType.PRINT_SERVERS_CHANGED, printServerStore);

        const printServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true
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
      assert(print_server_store_test.TestNames.GetPrintServersConfig),
      async () => {
        const printServers = [
          {id: 'server1', name: 'Print Server 1'},
          {id: 'server2', name: 'Print Server 2'},
        ];

        const expectedPrintServersConfig = {
          printServers: printServers,
          isSingleServerFetchingMode: true
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
  test(
      assert(print_server_store_test.TestNames.ServerPrintersLoading),
      async () => {
        const whenServerPrintersLoadedEvent = eventToPromise(
            PrintServerStore.EventType.SERVER_PRINTERS_LOADING,
            printServerStore);

        webUIListenerCallback('server-printers-loading', true);

        const serverPrintersLoadedEvent = await whenServerPrintersLoadedEvent;
        assertTrue(serverPrintersLoadedEvent.detail);
      });
});

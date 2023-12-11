// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ContainerInfo, ContainerSelectElement, CrostiniBrowserProxyImpl, CrostiniPortSetting, GuestOsBrowserProxyImpl, SettingsCrostiniExportImportElement, SettingsCrostiniPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let subpage: SettingsCrostiniExportImportElement;
let guestOsBrowserProxy: TestGuestOsBrowserProxy;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

const singleContainer: ContainerInfo[] = [
  {
    id: {
      vm_name: 'termina',
      container_name: 'penguin',
    },
    ipv4: '1.2.3.4',
  },
];

const multipleContainers: ContainerInfo[] = [
  {
    id: {
      vm_name: 'termina',
      container_name: 'penguin',
    },
    ipv4: '1.2.3.4',
  },
  {
    id: {
      vm_name: 'not-termina',
      container_name: 'not-penguin',

    },
    ipv4: '1.2.3.5',
  },
];

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

function setCrostiniPrefs(enabled: boolean, {
  sharedPaths = {},
  forwardedPorts = [],
  micAllowed = false,
  arcEnabled = false,
  bruschettaInstalled = false,
}: PrefParams = {}): void {
  crostiniPage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
    },
    bruschetta: {
      installed: {
        value: bruschettaInstalled,
      },
    },
    crostini: {
      enabled: {value: enabled},
      mic_allowed: {value: micAllowed},
      port_forwarding: {ports: {value: forwardedPorts}},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  flush();
}

function selectContainerByIndex(
    select: ContainerSelectElement, index: number): void {
  const mdSelect = select.shadowRoot!.querySelector<HTMLSelectElement>(
      'select#selectContainer.md-select');
  assertTrue(!!mdSelect);
  mdSelect.selectedIndex = index;
  mdSelect.dispatchEvent(new CustomEvent('change'));
  flush();
}

suite('<settings-crostini-export-import>', () => {
  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();

    disableAnimationsAndTransitions();

    setCrostiniPrefs(true, {arcEnabled: true});
    const requestInstallerStatusCallCount =
        crostiniBrowserProxy.getCallCount('requestCrostiniInstallerStatus');

    loadTimeData.overrideValues({
      showCrostiniExportImport: true,
      showCrostiniContainerUpgrade: true,
      showCrostiniPortForwarding: true,
      showCrostiniDiskResize: true,
      arcAdbSideloadingSupported: true,
      showCrostiniExtraContainers: true,
    });
    crostiniBrowserProxy.containerInfo = singleContainer;
    await flushTasks();

    Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT);

    await flushTasks();
    const subpageElement = crostiniPage.shadowRoot!.querySelector(
        'settings-crostini-export-import');
    assertTrue(!!subpageElement);
    subpage = subpageElement;
    assertEquals(
        1,
        crostiniBrowserProxy.getCallCount(
            'requestCrostiniExportImportOperationStatus'));
    assertEquals(
        requestInstallerStatusCallCount + 1,
        crostiniBrowserProxy.getCallCount('requestCrostiniInstallerStatus'));
    assertEquals(1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
  });

  teardown(() => {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to backup linux', async () => {
    const BACKUP_LINUX_APPS_AND_FILES_SETTING =
        settingMojom.Setting.kBackupLinuxAppsAndFiles.toString();
    const params = new URLSearchParams();
    params.append('settingId', BACKUP_LINUX_APPS_AND_FILES_SETTING);
    Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT, params);

    flush();
    const subpageElement = crostiniPage.shadowRoot!.querySelector(
        'settings-crostini-export-import');
    assertTrue(!!subpageElement);
    subpage = subpageElement;

    const deepLinkElement =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#export cr-button');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Export button should be focused for settingId=${
            BACKUP_LINUX_APPS_AND_FILES_SETTING}.`);
  });

  test('Export single container', () => {
    assertNull(
        subpage.shadowRoot!.querySelector('#exportCrostiniLabel .secondary'));
    const exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#export cr-button');
    assertTrue(!!exportBtn);
    exportBtn.click();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
  });

  test('Export multi container', async () => {
    crostiniBrowserProxy.containerInfo = multipleContainers;
    webUIListenerCallback('crostini-container-info', multipleContainers);
    await flushTasks();

    assertTrue(
        !!subpage.shadowRoot!.querySelector('#exportCrostiniLabel .secondary'));
    const select = subpage.shadowRoot!.querySelector<ContainerSelectElement>(
        '#exportContainerSelect');
    assertTrue(!!select);
    selectContainerByIndex(select, 1);

    const exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#export cr-button');
    assertTrue(!!exportBtn);
    exportBtn.click();

    assertEquals(
        1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
    const args = crostiniBrowserProxy.getArgs('exportCrostiniContainer');
    assertEquals(1, args.length);
    assertEquals('not-termina', args[0].vm_name);
    assertEquals('not-penguin', args[0].container_name);
  });

  test('Import single container', async () => {
    assertNull(
        subpage.shadowRoot!.querySelector('#importCrostiniLabel .secondary'));

    const importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#import cr-button');
    assertTrue(!!importBtn);
    importBtn.click();

    await flushTasks();
    const importConfirmationDialog = subpage.shadowRoot!.querySelector(
        'settings-crostini-import-confirmation-dialog');
    assertTrue(!!importConfirmationDialog);
    const continueBtn =
        importConfirmationDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            'cr-dialog cr-button[id="continue"]');
    assertTrue(!!continueBtn);
    continueBtn.click();
    assertEquals(
        1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
  });

  test('Import multi container', async () => {
    crostiniBrowserProxy.containerInfo = multipleContainers;
    webUIListenerCallback('crostini-container-info', multipleContainers);
    await flushTasks();

    assertTrue(
        !!subpage.shadowRoot!.querySelector('#importCrostiniLabel .secondary'));
    const select = subpage.shadowRoot!.querySelector<ContainerSelectElement>(
        '#importContainerSelect');
    assertTrue(!!select);
    selectContainerByIndex(select, 1);

    const importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#import cr-button');
    assertTrue(!!importBtn);
    importBtn.click();

    await flushTasks();
    const importConfirmationDialog = subpage.shadowRoot!.querySelector(
        'settings-crostini-import-confirmation-dialog');
    assertTrue(!!importConfirmationDialog);

    const continueBtn =
        importConfirmationDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            'cr-dialog cr-button[id="continue"]');
    assertTrue(!!continueBtn);
    continueBtn.click();

    assertEquals(
        1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
    const args = crostiniBrowserProxy.getArgs('importCrostiniContainer');
    assertEquals(1, args.length);
    assertEquals('not-termina', args[0].vm_name);
    assertEquals('not-penguin', args[0].container_name);
  });

  test('Export import buttons get disabled on operation status', async () => {
    let exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#export cr-button');
    assertTrue(!!exportBtn);

    let importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#import cr-button');
    assertTrue(!!importBtn);
    assertFalse(exportBtn.disabled);
    assertFalse(importBtn.disabled);
    webUIListenerCallback(
        'crostini-export-import-operation-status-changed', true);

    await flushTasks();
    let subpageElement = crostiniPage.shadowRoot!.querySelector(
        'settings-crostini-export-import');
    assertTrue(!!subpageElement);
    subpage = subpageElement;
    exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#export cr-button');
    assertTrue(!!exportBtn);
    importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#import cr-button');
    assertTrue(!!importBtn);
    assertTrue(exportBtn.disabled);
    assertTrue(importBtn.disabled);
    webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);

    await flushTasks();
    subpageElement = crostiniPage.shadowRoot!.querySelector(
        'settings-crostini-export-import');
    assertTrue(!!subpageElement);
    subpage = subpageElement;
    exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#export cr-button');
    assertTrue(!!exportBtn);
    importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#import cr-button');
    assertTrue(!!importBtn);
    assertFalse(exportBtn.disabled);
    assertFalse(importBtn.disabled);
  });

  test(
      'Export import buttons disabled on when installing crostini',
      async () => {
        let exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#export cr-button');
        assertTrue(!!exportBtn);
        let importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#import cr-button');
        assertTrue(!!importBtn);

        assertFalse(exportBtn.disabled);
        assertFalse(importBtn.disabled);
        webUIListenerCallback('crostini-installer-status-changed', true);

        await flushTasks();
        let subpageElement = crostiniPage.shadowRoot!.querySelector(
            'settings-crostini-export-import');
        assertTrue(!!subpageElement);
        subpage = subpageElement;
        exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#export cr-button');
        assertTrue(!!exportBtn);
        importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#import cr-button');
        assertTrue(!!importBtn);

        assertTrue(exportBtn.disabled);
        assertTrue(importBtn.disabled);
        webUIListenerCallback('crostini-installer-status-changed', false);

        await flushTasks();
        subpageElement = crostiniPage.shadowRoot!.querySelector(
            'settings-crostini-export-import');
        assertTrue(!!subpageElement);
        subpage = subpageElement;
        exportBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#export cr-button');
        assertTrue(!!exportBtn);
        importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#import cr-button');
        assertTrue(!!importBtn);

        assertFalse(exportBtn.disabled);
        assertFalse(importBtn.disabled);
      });
});

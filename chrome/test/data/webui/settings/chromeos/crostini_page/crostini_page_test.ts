// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {BruschettaSubpageElement, ContainerInfo, ContainerSelectElement, CrostiniBrowserProxyImpl, CrostiniPortForwardingElement, CrostiniPortSetting, CrostiniSharedUsbDevicesElement, GuestOsBrowserProxyImpl, SettingsCrostiniArcAdbElement, SettingsCrostiniConfirmationDialogElement, SettingsCrostiniDiskResizeDialogElement, SettingsCrostiniExportImportElement, SettingsCrostiniPageElement, SettingsCrostiniSubpageElement, SettingsGuestOsSharedPathsElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrSliderElement, CrToastElement, CrToggleElement, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGE, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let guestOsBrowserProxy: TestGuestOsBrowserProxy;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

const MIC_ALLOWED_PATH = 'prefs.crostini.mic_allowed.value';

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

suite('<settings-crostini-page>', () => {
  setup(() => {
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
  });

  teardown(() => {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('<settings-crostini-confirmation-dialog>', () => {
    let dialog: SettingsCrostiniConfirmationDialogElement;
    let cancelOrCloseEvents: CustomEvent[];
    let closeEventPromise: Promise<Event>;

    setup(() => {
      cancelOrCloseEvents = [];
      dialog = document.createElement('settings-crostini-confirmation-dialog');

      dialog.addEventListener('cancel', (e: Event) => {
        cancelOrCloseEvents.push(e as CustomEvent);
      });
      closeEventPromise = new Promise(
          (resolve) => dialog.addEventListener('close', (e: Event) => {
            cancelOrCloseEvents.push(e as CustomEvent);
            resolve(e);
          }));

      document.body.appendChild(dialog);
    });

    teardown(() => {
      dialog.remove();
    });

    test('accept', async () => {
      let crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertTrue(crDialogElement.open);
      const actionButton =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!actionButton);
      actionButton.click();

      await closeEventPromise;
      assertEquals(1, cancelOrCloseEvents.length);
      assertEquals('close', cancelOrCloseEvents[0]!.type);
      assertTrue(cancelOrCloseEvents[0]!.detail.accepted);
      crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertFalse(crDialogElement.open);
    });

    test('cancel', async () => {
      let crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertTrue(crDialogElement.open);
      const cancelButton =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancelButton);
      cancelButton.click();

      await closeEventPromise;
      assertEquals(2, cancelOrCloseEvents.length);
      assertEquals('cancel', cancelOrCloseEvents[0]!.type);
      assertEquals('close', cancelOrCloseEvents[1]!.type);
      assertFalse(cancelOrCloseEvents[1]!.detail.accepted);
      crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertFalse(crDialogElement.open);
    });
  });

  suite('Subpage details', () => {
    let subpage: SettingsCrostiniSubpageElement;

    setup(async () => {
      setCrostiniPrefs(true, {arcEnabled: true});
      loadTimeData.overrideValues({
        showCrostiniExportImport: true,
        showCrostiniContainerUpgrade: true,
        showCrostiniPortForwarding: true,
        showCrostiniDiskResize: true,
        arcAdbSideloadingSupported: true,
        showCrostiniExtraContainers: true,
      });

      Router.getInstance().navigateTo(routes.CROSTINI);
      const crostiniSettingsCard =
          crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
      assertTrue(!!crostiniSettingsCard);
      const button =
          crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
              '#crostini');
      assertTrue(!!button);
      button.click();

      await flushTasks();
      const subpageElement =
          crostiniPage.shadowRoot!.querySelector('settings-crostini-subpage');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    suite('Subpage default', () => {
      test('Basic', () => {
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniSharedPathsRow'));
        assertTrue(!!subpage.shadowRoot!.querySelector(
            '#crostiniSharedUsbDevicesRow'));
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniExportImportRow'));
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniEnableArcAdbRow'));
        assertTrue(!!subpage.shadowRoot!.querySelector('#remove'));
        assertTrue(!!subpage.shadowRoot!.querySelector('#container-upgrade'));
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniPortForwardingRow'));
        assertTrue(!!subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-toggle'));
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniDiskResizeRow'));
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#crostiniExtraContainersRow'));
      });

      test('Shared paths', async () => {
        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#crostiniSharedPathsRow');
        assertTrue(!!button);
        button.click();

        await flushTasks();
        const sharedPathsPage = crostiniPage.shadowRoot!.querySelector(
            'settings-guest-os-shared-paths');
        assertTrue(!!sharedPathsPage);
      });

      test('Container upgrade', () => {
        const crButton = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#container-upgrade cr-button');
        assertTrue(!!crButton);
        crButton.click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniContainerUpgradeView'));
      });

      test('Container upgrade button disabled on upgrade dialog', async () => {
        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#container-upgrade cr-button');
        assertTrue(!!button);

        await flushTasks();
        assertFalse(button.disabled);
        webUIListenerCallback('crostini-upgrader-status-changed', true);

        await flushTasks();
        assertTrue(button.disabled);
        webUIListenerCallback('crostini-upgrader-status-changed', false);

        await flushTasks();
        assertFalse(button.disabled);
      });

      test('Container upgrade button disabled on install', async () => {
        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#container-upgrade cr-button');
        assertTrue(!!button);

        await flushTasks();
        assertFalse(button.disabled);
        webUIListenerCallback('crostini-installer-status-changed', true);

        await flushTasks();
        assertTrue(button.disabled);
        webUIListenerCallback('crostini-installer-status-changed', false);

        await flushTasks();
        assertFalse(button.disabled);
      });

      test('Installer status queried on attach', () => {
        // We navigated the page during setup, so this request should've been
        // triggered by here.
        assertGE(
            crostiniBrowserProxy.getCallCount('requestCrostiniInstallerStatus'),
            1);
      });

      test('Toggle crostini mic permission cancel', async () => {
        // Crostini is assumed to be running when the page is loaded.
        let toggle =
            subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#crostini-mic-permission-toggle');
        assertTrue(!!toggle);
        let dialog = subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog');
        assertNull(dialog);

        setCrostiniPrefs(true, {micAllowed: true});
        assertTrue(toggle.checked);

        toggle.click();
        await flushTasks();

        dialog = subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog');
        assertTrue(!!dialog);
        const dialogClosedPromise = eventToPromise('close', dialog);
        const cancelBtn = dialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.cancel-button');
        assertTrue(!!cancelBtn);
        cancelBtn.click();
        await Promise.all([dialogClosedPromise, flushTasks()]);

        // Because the dialog was cancelled, the toggle should not have changed.
        assertNull(subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog'));

        toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crostini-mic-permission-toggle');
        assertTrue(!!toggle);
        assertTrue(toggle.checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('Toggle crostini mic permission shutdown', async () => {
        // Crostini is assumed to be running when the page is loaded.
        let toggle =
            subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#crostini-mic-permission-toggle');
        assertTrue(!!toggle);
        let dialog = subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog');
        assertNull(dialog);

        setCrostiniPrefs(true, {micAllowed: false});

        assertFalse(toggle.checked);

        toggle.click();
        await flushTasks();
        dialog = subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog');
        assertTrue(!!dialog);
        const dialogClosedPromise = eventToPromise('close', dialog);
        const actionBtn = dialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
        assertTrue(!!actionBtn);
        actionBtn.click();
        await Promise.all([dialogClosedPromise, flushTasks()]);
        assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
        assertNull(subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog'));
        toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crostini-mic-permission-toggle');
        assertTrue(!!toggle);
        assertTrue(toggle.checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));

        // Crostini is now shutdown, this means that it doesn't need to be
        // restarted in order for changes to take effect, therefore no dialog is
        // needed and the mic sharing settings can be changed immediately.
        toggle.click();
        await flushTasks();
        assertNull(subpage.shadowRoot!.querySelector(
            '#crostini-mic-permission-dialog'));
        assertFalse(toggle.checked);
        assertFalse(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('Remove', async () => {
        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#remove cr-button');
        assertTrue(!!button);
        button.click();

        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await flushTasks();
        assertEquals(routes.CROSTINI, Router.getInstance().currentRoute);

        const crostiniSettingsCard =
            crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
        assertTrue(!!crostiniSettingsCard);
        assertTrue(!!crostiniSettingsCard.shadowRoot!.querySelector(
            '#enableCrostiniButton'));
      });

      test('Remove hidden', async () => {
        // Elements are not destroyed when a dom-if stops being shown, but we
        // can check if their rendered width is non-zero. This should be
        // resilient against most formatting changes, since we're not relying on
        // them having any exact size, or on Polymer using any particular means
        // of hiding elements.
        let removeElement = subpage.shadowRoot!.querySelector('#remove');
        assertTrue(isVisible(removeElement));
        webUIListenerCallback('crostini-installer-status-changed', true);

        await flushTasks();
        removeElement = subpage.shadowRoot!.querySelector('#remove');
        assertTrue(!!removeElement);
        assertEquals(0, removeElement.clientWidth);
        webUIListenerCallback('crostini-installer-status-changed', false);

        await flushTasks();
        removeElement = subpage.shadowRoot!.querySelector('#remove');
        assertTrue(isVisible(removeElement));
      });

      test('Hide on disable', async () => {
        assertEquals(
            routes.CROSTINI_DETAILS, Router.getInstance().currentRoute);
        setCrostiniPrefs(false);

        await eventToPromise('popstate', window);
        assertEquals(routes.CROSTINI, Router.getInstance().currentRoute);
      });

      test('Disk resize opens when clicked', async () => {
        const showDiskResizeButton =
            subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#showDiskResizeButton');
        assertTrue(!!showDiskResizeButton);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});
        showDiskResizeButton.click();

        await flushTasks();
        const dialog = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
      });

      test('Deep link to resize disk', async () => {
        assertTrue(
            !!subpage.shadowRoot!.querySelector('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});

        const CROSTINI_DISK_RESIZE_SETTING =
            settingMojom.Setting.kCrostiniDiskResize.toString();
        const params = new URLSearchParams();
        params.append('settingId', CROSTINI_DISK_RESIZE_SETTING);
        Router.getInstance().navigateTo(routes.CROSTINI_DETAILS, params);

        const deepLinkElement =
            subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#showDiskResizeButton');
        assertTrue(!!deepLinkElement);
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            `Resize disk button should be focused for settingId=${
                CROSTINI_DISK_RESIZE_SETTING}.`);
      });
    });

    suite('Subpage backup restore', () => {
      let subpage: SettingsCrostiniExportImportElement;

      setup(async () => {
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
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniInstallerStatus'));
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
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
        assertNull(subpage.shadowRoot!.querySelector(
            '#exportCrostiniLabel .secondary'));
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

        assertTrue(!!subpage.shadowRoot!.querySelector(
            '#exportCrostiniLabel .secondary'));
        const select =
            subpage.shadowRoot!.querySelector<ContainerSelectElement>(
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
        assertNull(subpage.shadowRoot!.querySelector(
            '#importCrostiniLabel .secondary'));

        const importBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#import cr-button');
        assertTrue(!!importBtn);
        importBtn.click();

        await flushTasks();
        const importConfirmationDialog = subpage.shadowRoot!.querySelector(
            'settings-crostini-import-confirmation-dialog');
        assertTrue(!!importConfirmationDialog);
        const continueBtn = importConfirmationDialog.shadowRoot!
                                .querySelector<HTMLButtonElement>(
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

        assertTrue(!!subpage.shadowRoot!.querySelector(
            '#importCrostiniLabel .secondary'));
        const select =
            subpage.shadowRoot!.querySelector<ContainerSelectElement>(
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

        const continueBtn = importConfirmationDialog.shadowRoot!
                                .querySelector<HTMLButtonElement>(
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

      test(
          'Export import buttons get disabled on operation status',
          async () => {
            let exportBtn =
                subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                    '#export cr-button');
            assertTrue(!!exportBtn);

            let importBtn =
                subpage.shadowRoot!.querySelector<HTMLButtonElement>(
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
            let exportBtn =
                subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                    '#export cr-button');
            assertTrue(!!exportBtn);
            let importBtn =
                subpage.shadowRoot!.querySelector<HTMLButtonElement>(
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

    suite('Subpage port forwarding', () => {
      let subpage: CrostiniPortForwardingElement;

      const allContainers: ContainerInfo[] = [
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
      setup(async () => {
        crostiniBrowserProxy.portOperationSuccess = true;
        crostiniBrowserProxy.containerInfo = allContainers;
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
            {
              port_number: 5001,
              protocol_type: 1,
              label: 'Label2',
              vm_name: 'not-termina',
              container_name: 'not-penguin',
              container_id: {
                vm_name: 'not-termina',
                container_name: 'not-penguin',
              },
              is_active: false,
            },
          ],
        });

        await flushTasks();
        Router.getInstance().navigateTo(routes.CROSTINI_PORT_FORWARDING);

        await flushTasks();
        const subpageElement = crostiniPage.shadowRoot!.querySelector(
            'settings-crostini-port-forwarding');
        assertTrue(!!subpageElement);
        subpage = subpageElement;
        assertTrue(!!subpage);
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
      });

      test('Display ports', () => {
        // Extra list item for the titles.
        assertEquals(
            4, subpage.shadowRoot!.querySelectorAll('.list-item').length);
      });

      test('Add port success', async () => {
        const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPort cr-button');
        assertTrue(!!addPortBtn);
        addPortBtn.click();

        await flushTasks();
        const addPortDialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-add-port-dialog');
        assertTrue(!!addPortDialogElement);
        const portNumberInput =
            addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
                '#portNumberInput');
        assertTrue(!!portNumberInput);
        portNumberInput.focus();
        portNumberInput.value = '5002';
        portNumberInput.blur();
        assertFalse(portNumberInput.invalid);
        const portLabelInput =
            addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
                '#portLabelInput');
        assertTrue(!!portLabelInput);
        portLabelInput.value = 'Some Label';
        const select = addPortDialogElement.shadowRoot!.querySelector(
            'settings-guest-os-container-select');
        assertTrue(!!select);
        selectContainerByIndex(select, 1);

        const continueButton =
            addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
                'cr-dialog cr-button[id="continue"]');
        assertTrue(!!continueButton);
        continueButton.click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        const args = crostiniBrowserProxy.getArgs('addCrostiniPortForward')[0];
        assertEquals(4, args.length);
        assertEquals('not-termina', args[0].vm_name);
        assertEquals('not-penguin', args[0].container_name);
      });

      test('Add port fail', async () => {
        const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPort cr-button');
        assertTrue(!!addPortBtn);
        addPortBtn.click();

        await flushTasks();
        const addPortDialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-add-port-dialog');
        assertTrue(!!addPortDialogElement);
        const portNumberInput =
            addPortDialogElement.shadowRoot!.querySelector<CrInputElement>(
                '#portNumberInput');
        assertTrue(!!portNumberInput);
        const continueButton =
            addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
                'cr-dialog cr-button[id="continue"]');
        assertTrue(!!continueButton);

        assertFalse(portNumberInput.invalid);
        portNumberInput.focus();
        portNumberInput.value = '1023';
        continueButton.click();
        assertEquals(
            0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        assertTrue(continueButton.disabled);
        assertTrue(portNumberInput.invalid);
        assertEquals(
            loadTimeData.getString('crostiniPortForwardingAddError'),
            portNumberInput.errorMessage);

        portNumberInput.value = '65536';
        assertTrue(continueButton.disabled);
        assertTrue(portNumberInput.invalid);
        assertEquals(
            loadTimeData.getString('crostiniPortForwardingAddError'),
            portNumberInput.errorMessage);

        portNumberInput.focus();
        portNumberInput.value = '5000';
        portNumberInput.blur();

        continueButton.click();
        assertTrue(continueButton.disabled);
        assertTrue(portNumberInput.invalid);
        assertEquals(
            loadTimeData.getString('crostiniPortForwardingAddExisting'),
            portNumberInput.errorMessage);

        portNumberInput.focus();
        portNumberInput.value = '1024';
        portNumberInput.blur();
        assertFalse(continueButton.disabled);
        assertFalse(portNumberInput.invalid);
        assertEquals('', portNumberInput.errorMessage);
      });

      test('Add port cancel', async () => {
        const addPortBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPort cr-button');
        assertTrue(!!addPortBtn);
        addPortBtn.click();

        await flushTasks();
        const addPortDialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-add-port-dialog');
        assertTrue(!!addPortDialogElement);
        const cancelBtn =
            addPortDialogElement.shadowRoot!.querySelector<HTMLButtonElement>(
                'cr-dialog cr-button[id="cancel"]');
        assertTrue(!!cancelBtn);
        cancelBtn.click();

        await flushTasks();
        assertTrue(!!crostiniPage.shadowRoot!.querySelector(
            'settings-crostini-port-forwarding'));
      });

      test('Remove all ports', async () => {
        const showMenuBtn =
            subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#showRemoveAllPortsMenu');
        assertTrue(!!showMenuBtn);
        showMenuBtn.click();

        await flushTasks();
        const removeBtn = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#removeAllPortsButton');
        assertTrue(!!removeBtn);
        removeBtn.click();
        assertEquals(
            2,
            crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
      });

      test('Remove single port', () => {
        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#removeSinglePortButton0-0');
        assertTrue(!!button);
        button.click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
        const args =
            crostiniBrowserProxy.getArgs('removeCrostiniPortForward')[0];
        assertEquals(3, args.length);
        assertEquals('termina', args[0].vm_name);
        assertEquals('penguin', args[0].container_name);
      });

      test('Activate single port success', async () => {
        let errorToast =
            subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
        assertTrue(!!errorToast);
        assertFalse(errorToast.open);
        await flushTasks();

        const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#toggleActivationButton0-0');
        assertTrue(!!crToggle);
        assertFalse(crToggle.disabled);
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        errorToast =
            subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
        assertTrue(!!errorToast);
        assertFalse(errorToast.open);
      });

      test('Activate single port fail', async () => {
        crostiniBrowserProxy.portOperationSuccess = false;
        let errorToast =
            subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
        assertTrue(!!errorToast);
        assertFalse(errorToast.open);

        const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#toggleActivationButton1-0');
        assertTrue(!!crToggle);
        assertFalse(crToggle.disabled);
        assertFalse(crToggle.checked);
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertFalse(crToggle.checked);
        errorToast =
            subpage.shadowRoot!.querySelector<CrToastElement>('#errorToast');
        assertTrue(!!errorToast);
        assertTrue(errorToast.open);
      });

      test('Deactivate single port', async () => {
        const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#toggleActivationButton0-0');
        assertTrue(!!crToggle);
        assertFalse(crToggle.disabled);
        crToggle.checked = true;
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('deactivateCrostiniPortForward'));
      });

      test('Active ports changed', async () => {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
          ],
        });
        const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#toggleActivationButton0-0');
        assertTrue(!!crToggle);

        webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed',
            [{'port_number': 5000, 'protocol_type': 0}]);
        await flushTasks();
        assertTrue(crToggle.checked);

        webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed', []);
        await flushTasks();
        assertFalse(crToggle.checked);
      });

      test('Port prefs change', () => {
        // Default prefs should have list items per port, plus one per
        // container.
        assertEquals(
            4, subpage.shadowRoot!.querySelectorAll('.list-item').length);

        // When only one of the default container has ports, we lose an item for
        // the extra container heading.
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label2',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
          ],
        });
        assertEquals(
            3, subpage.shadowRoot!.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label2',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
            {
              port_number: 5002,
              protocol_type: 0,
              label: 'Label3',
              vm_name: 'termina',
              container_name: 'penguin',
              container_id: {
                vm_name: 'termina',
                container_name: 'penguin',
              },
              is_active: false,
            },
          ],
        });
        assertEquals(
            4, subpage.shadowRoot!.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {forwardedPorts: []});
        assertEquals(
            0, subpage.shadowRoot!.querySelectorAll('.list-item').length);
      });

      test('Container stop and start', async () => {
        const crToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#toggleActivationButton0-0');
        assertTrue(!!crToggle);
        assertFalse(crToggle.disabled);

        allContainers[0]!.ipv4 = null;
        webUIListenerCallback(
            'crostini-container-info', structuredClone(allContainers));
        await flushTasks();
        assertTrue(crToggle.disabled);

        allContainers[0]!.ipv4 = '1.2.3.4';
        webUIListenerCallback(
            'crostini-container-info', structuredClone(allContainers));
        await flushTasks();
        assertFalse(crToggle.disabled);
      });
    });

    suite('Disk resize', () => {
      let dialog: SettingsCrostiniDiskResizeDialogElement;
      /**
       * Helper function to assert that the expected block is visible and the
       * others are not.
       */
      function assertVisibleBlockIs(selector: string): void {
        const selectors =
            ['#unsupported', '#resize-block', '#error', '#loading'];

        assertTrue(isVisible(dialog.shadowRoot!.querySelector(selector)));
        selectors.filter(s => s !== selector).forEach(s => {
          assertFalse(isVisible(dialog.shadowRoot!.querySelector(s)));
        });
      }

      const ticks = [
        {label: 'label 0', value: 0, ariaLabel: 'label 0'},
        {label: 'label 10', value: 10, ariaLabel: 'label 10'},
        {label: 'label 100', value: 100, ariaLabel: 'label 100'},
      ];

      const resizeableData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: true,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks,
      };

      const sparseDiskData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: false,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks,
      };

      async function clickShowDiskResize(userChosen: boolean): Promise<void> {
        await crostiniBrowserProxy.resolvePromises('getCrostiniDiskInfo', {
          succeeded: true,
          canResize: true,
          isUserChosenSize: userChosen,
          ticks,
          defaultIndex: 2,
        });

        const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#showDiskResizeButton');
        assertTrue(!!button);
        button.click();
        await flushTasks();

        const dialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-dialog');

        if (userChosen) {
          // We should be on the loading page but unable to kick off a resize
          // yet.
          assertTrue(!!dialogElement);
          dialog = dialogElement;
          assertTrue(!!dialog.shadowRoot!.querySelector('#loading'));
          const resizeBtn =
              dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
          assertTrue(!!resizeBtn);
          assertTrue(resizeBtn.disabled);
        }
      }

      test('Resize unsupported', async () => {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: true, canResize: false});
        assertFalse(isVisible(
            subpage.shadowRoot!.querySelector('#showDiskResizeButton')));
        const subtext = subpage.shadowRoot!.querySelector<HTMLElement>(
            '#diskSizeDescription');
        assertTrue(!!subtext);
        assertEquals(
            loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext'),
            subtext.innerText);
      });

      test('Resize button and subtext correctly set', async () => {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = subpage.shadowRoot!.querySelector<HTMLElement>(
            '#showDiskResizeButton');
        const subtext = subpage.shadowRoot!.querySelector<HTMLElement>(
            '#diskSizeDescription');
        assertTrue(!!button);
        assertTrue(!!subtext);

        assertEquals(
            loadTimeData.getString('crostiniDiskResizeShowButton'),
            button.innerText);
        assertEquals('label 100', subtext.innerText);
      });

      test('Reserve size button and subtext correctly set', async () => {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        const button = subpage.shadowRoot!.querySelector<HTMLElement>(
            '#showDiskResizeButton');
        const subtext = subpage.shadowRoot!.querySelector<HTMLElement>(
            '#diskSizeDescription');
        assertTrue(!!button);
        assertTrue(!!subtext);

        assertEquals(
            loadTimeData.getString('crostiniDiskReserveSizeButton'),
            button.innerText);
        assertEquals(
            loadTimeData.getString(
                'crostiniDiskResizeDynamicallyAllocatedSubtext'),
            subtext.innerText);
      });

      test('Resize recommendation shown correctly', async () => {
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertTrue(
            isVisible(dialog.shadowRoot!.querySelector('#recommended-size')));
        assertFalse(isVisible(
            dialog.shadowRoot!.querySelector('#recommended-size-warning')));
      });

      test('Resize recommendation warning shown correctly', async () => {
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        diskInfo.isLowSpaceAvailable = true;
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertFalse(
            isVisible(dialog.shadowRoot!.querySelector('#recommended-size')));
        assertTrue(isVisible(
            dialog.shadowRoot!.querySelector('#recommended-size-warning')));
      });

      test('Message shown if error and can retry', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: false, isUserChosenSize: true});

        // We failed, should have a retry button.
        let button =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#retry');
        assertVisibleBlockIs('#error');
        assertTrue(!!button);

        let resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        assertTrue(resizeBtn.disabled);

        let cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);

        // Back to the loading screen.
        button.click();
        await flushTasks();
        assertVisibleBlockIs('#loading');

        resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        assertTrue(resizeBtn.disabled);

        cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);

        // And failure page again.
        await crostiniBrowserProxy.rejectPromises('getCrostiniDiskInfo');
        button = dialog.shadowRoot!.querySelector('#retry');
        assertTrue(isVisible(button));
        assertVisibleBlockIs('#error');

        resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        assertTrue(resizeBtn.disabled);

        cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);
      });

      test('Message shown if cannot resize', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: false, isUserChosenSize: true});
        assertVisibleBlockIs('#unsupported');

        const resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        assertTrue(resizeBtn.disabled);

        const cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);
      });

      test('Resize page shown if can resize', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        assertVisibleBlockIs('#resize-block');

        const labelBegin =
            dialog.shadowRoot!.querySelector<HTMLElement>('#label-begin');
        assertTrue(!!labelBegin);
        assertEquals(ticks[0]!.label, labelBegin.innerText);

        const labelEnd =
            dialog.shadowRoot!.querySelector<HTMLElement>('#label-end');
        assertTrue(!!labelEnd);
        assertEquals(ticks[2]!.label, labelEnd.innerText);

        const diskSlider =
            dialog.shadowRoot!.querySelector<CrSliderElement>('#diskSlider');
        assertTrue(!!diskSlider);
        assertEquals(2, diskSlider.value);

        const resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        assertFalse(resizeBtn.disabled);

        const cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);
      });

      test('In progress resizing', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const resizeBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!resizeBtn);
        resizeBtn.click();
        await flushTasks();
        assertTrue(resizeBtn.disabled);
        assertFalse(isVisible(dialog.shadowRoot!.querySelector('#done')));
        assertTrue(isVisible(dialog.shadowRoot!.querySelector('#resizing')));
        assertFalse(
            isVisible(dialog.shadowRoot!.querySelector('#resize-error')));
        const cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertTrue(cancelBtn.disabled);
      });

      test('Error resizing', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!button);
        button.click();
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', false);
        assertFalse(button.disabled);

        assertFalse(isVisible(dialog.shadowRoot!.querySelector('#done')));
        assertFalse(isVisible(dialog.shadowRoot!.querySelector('#resizing')));
        assertTrue(
            isVisible(dialog.shadowRoot!.querySelector('#resize-error')));

        const cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        assertFalse(cancelBtn.disabled);
      });

      test('Success resizing', async () => {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#resize');
        assertTrue(!!button);
        button.click();
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', true);
        // Dialog should close itself.
        await eventToPromise('close', dialog);
      });

      test('Disk resize confirmation dialog shown and accepted', async () => {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);
        // Dismiss confirmation.
        let confirmationDialog = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(!!confirmationDialog);
        assertTrue(
            isVisible(confirmationDialog.shadowRoot!.querySelector('#cancel')));
        let continueBtn =
            confirmationDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#continue');
        assertTrue(!!continueBtn);
        continueBtn.click();
        await eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        let dialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialogElement);
        dialog = dialogElement;
        assertTrue(isVisible(dialog.shadowRoot!.querySelector('#resize')));

        // Cancel main resize dialog.
        const cancelBtn =
            dialog.shadowRoot!.querySelector<HTMLButtonElement>('#cancel');
        assertTrue(!!cancelBtn);
        cancelBtn.click();
        await eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        await clickShowDiskResize(false);
        confirmationDialog = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(!!confirmationDialog);
        continueBtn =
            confirmationDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#continue');
        assertTrue(!!continueBtn);
        continueBtn.click();
        await eventToPromise('close', confirmationDialog);

        // Main dialog should show again.
        dialogElement = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialogElement);
        dialog = dialogElement;
        assertTrue(isVisible(dialog.shadowRoot!.querySelector('#resize')));
        assertTrue(isVisible(dialog.shadowRoot!.querySelector('#cancel')));
      });

      test('Disk resize confirmation dialog shown and canceled', async () => {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);

        const confirmationDialog = subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(!!confirmationDialog);
        assertTrue(isVisible(
            confirmationDialog.shadowRoot!.querySelector('#continue')));

        const cancelBtn =
            confirmationDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#cancel');
        assertTrue(!!cancelBtn);
        cancelBtn.click();
        await eventToPromise('close', confirmationDialog);

        assertNull(subpage.shadowRoot!.querySelector(
            'settings-crostini-disk-resize-dialog'));
      });
    });
  });

  suite('Bruschetta subpage', () => {
    let subpage: BruschettaSubpageElement;

    setup(async () => {
      Router.getInstance().navigateTo(routes.CROSTINI);
      setCrostiniPrefs(false, {bruschettaInstalled: true});
      flush();

      const crostiniSettingsCard =
          crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
      assertTrue(!!crostiniSettingsCard);
      crostiniSettingsCard.set('showBruschetta_', true);
      await flushTasks();
      const button =
          crostiniSettingsCard.shadowRoot!.querySelector<HTMLButtonElement>(
              '#bruschetta');
      assertTrue(!!button);
      button.click();

      await flushTasks();
      const subpageElement =
          crostiniPage.shadowRoot!.querySelector('settings-bruschetta-subpage');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('Navigate to shared USB devices', async () => {
      const link = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#bruschettaSharedUsbDevicesRow');
      assertTrue(!!link);
      link.click();
      flush();

      assertEquals(
          routes.BRUSCHETTA_SHARED_USB_DEVICES,
          Router.getInstance().currentRoute);

      assertTrue(!!crostiniPage.shadowRoot!.querySelector(
          'settings-guest-os-shared-usb-devices[guest-os-type="bruschetta"]'));
      // Functionality is tested in guest_os_shared_usb_devices_test.js

      // Navigate back
      const popStateEventPromise = eventToPromise('popstate', window);
      Router.getInstance().navigateToPreviousRoute();
      await popStateEventPromise;
      await waitAfterNextRender(subpage);

      assertEquals(
          link, subpage.shadowRoot!.activeElement,
          `${link} should be focused.`);
    });

    test('Navigate to shared paths', async () => {
      const link = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#bruschettaSharedPathsRow');
      assertTrue(!!link);
      link.click();
      flush();

      assertEquals(
          routes.BRUSCHETTA_SHARED_PATHS, Router.getInstance().currentRoute);

      assertTrue(!!crostiniPage.shadowRoot!.querySelector(
          'settings-guest-os-shared-paths[guest-os-type="bruschetta"]'));
      // Functionality is tested in guest_os_shared_paths_test.js

      // Navigate back
      const popStateEventPromise = eventToPromise('popstate', window);
      Router.getInstance().navigateToPreviousRoute();
      await popStateEventPromise;
      await waitAfterNextRender(subpage);

      assertEquals(
          link, subpage.shadowRoot!.activeElement,
          `${link} should be focused.`);
    });

    test('Remove bruschetta', async () => {
      const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#remove cr-button');
      assertTrue(!!button);
      assertFalse(button.disabled);
      button.click();
      flush();

      assertEquals(
          1,
          crostiniBrowserProxy.getCallCount(
              'requestBruschettaUninstallerView'));
      setCrostiniPrefs(false, {bruschettaInstalled: false});

      await eventToPromise('popstate', window);

      assertEquals(routes.CROSTINI, Router.getInstance().currentRoute);

      const crostiniSettingsCard =
          crostiniPage.shadowRoot!.querySelector('crostini-settings-card');
      assertTrue(!!crostiniSettingsCard);
      assertTrue(!!crostiniSettingsCard.shadowRoot!.querySelector(
          '#enableBruschettaButton'));
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedPathsTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  suite('Subpage shared paths', () => {
    let subpage: SettingsGuestOsSharedPathsElement;

    setup(async () => {
      setCrostiniPrefs(
          true, {sharedPaths: {path1: ['termina'], path2: ['some-other-vm']}});

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);

      await flushTasks();
      const subpageElement = crostiniPage.shadowRoot!.querySelector(
          'settings-guest-os-shared-paths');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
      await flushTasks();
    });

    test('Basic', () => {
      assertEquals(
          1, subpage.shadowRoot!.querySelectorAll('.list-item').length);
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  suite('Subpage shared Usb devices', () => {
    let subpage: CrostiniSharedUsbDevicesElement;

    setup(async () => {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExtraContainers: false,
      });
      guestOsBrowserProxy.sharedUsbDevices = [
        {
          guid: '0001',
          label: 'usb_dev1',
          guestId: {
            vm_name: 'termina',
            container_name: '',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: false,
        },
        {
          guid: '0002',
          label: 'usb_dev2',
          guestId: {
            vm_name: '',
            container_name: '',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: false,
        },
      ];

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      const subpageElement = crostiniPage.shadowRoot!.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('USB devices are shown', () => {
      const items =
          subpage.shadowRoot!.querySelectorAll<CrToggleElement>('.toggle');
      assertEquals(2, items.length);
      assertTrue(items[0]!.checked);
      assertFalse(items[1]!.checked);
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page.
  suite('Subpage shared Usb devices multi container', () => {
    let subpage: CrostiniSharedUsbDevicesElement;

    setup(async () => {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExtraContainers: true,
      });
      crostiniBrowserProxy.containerInfo = multipleContainers;
      guestOsBrowserProxy.sharedUsbDevices = [
        {
          guid: '0001',
          label: 'usb_dev1',
          guestId: {
            vm_name: '',
            container_name: '',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: false,
        },
        {
          guid: '0002',
          label: 'usb_dev2',
          guestId: {
            vm_name: 'termina',
            container_name: 'penguin',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: true,
        },
        {
          guid: '0003',
          label: 'usb_dev3',
          guestId: {
            vm_name: 'not-termina',
            container_name: 'not-penguin',
          },
          vendorId: '0000',
          productId: '0000',
          promptBeforeSharing: true,
        },
      ];

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      const subpageElement = crostiniPage.shadowRoot!.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('USB devices are shown', () => {
      const guests = subpage.shadowRoot!.querySelectorAll<HTMLElement>(
          '.usb-list-guest-id');
      assertEquals(2, guests.length);
      assertEquals('penguin', guests[0]!.innerText);
      assertEquals('not-termina:not-penguin', guests[1]!.innerText);

      const devices = subpage.shadowRoot!.querySelectorAll<HTMLElement>(
          '.usb-list-card-label');
      assertEquals(2, devices.length);
      assertEquals('usb_dev2', devices[0]!.innerText);
      assertEquals('usb_dev3', devices[1]!.innerText);
    });
  });

  suite('Subpage arc adb', () => {
    let subpage: SettingsCrostiniArcAdbElement;

    setup(async () => {
      setCrostiniPrefs(true, {arcEnabled: true});
      loadTimeData.overrideValues({
        arcAdbSideloadingSupported: true,
      });

      await flushTasks();
      Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB);

      await flushTasks();
      const subpageElement =
          crostiniPage.shadowRoot!.querySelector('settings-crostini-arc-adb');
      assertTrue(!!subpageElement);
      subpage = subpageElement;
    });

    test('Deep link to enable adb debugging', async () => {
      const CROSTINI_ADB_DEBUGGING_SETTING =
          settingMojom.Setting.kCrostiniAdbDebugging.toString();
      const params = new URLSearchParams();
      params.append('settingId', CROSTINI_ADB_DEBUGGING_SETTING);
      Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB, params);

      flush();

      const deepLinkElement =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#arcAdbEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Enable adb debugging button should be focused for settingId=${
              CROSTINI_ADB_DEBUGGING_SETTING}.`);
    });
  });
});

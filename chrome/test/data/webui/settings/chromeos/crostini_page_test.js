// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrostiniBrowserProxyImpl, GuestOsBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestGuestOsBrowserProxy} from './guest_os/test_guest_os_browser_proxy.js';
import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestGuestOsBrowserProxy} */
let guestOsBrowserProxy = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

const MIC_ALLOWED_PATH = 'prefs.crostini.mic_allowed.value';

const singleContainer = /** @type {!Array<!ContainerInfo>}*/
    ([
      {
        id: {
          vm_name: 'termina',
          container_name: 'penguin',
        },
        ipv4: '1.2.3.4',
      },
    ]);

const multipleContainers = /** @type {!Array<!ContainerInfo>}*/
    ([
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
    ]);

function setCrostiniPrefs(enabled, optional = {}) {
  const {
    sharedPaths = {},
    forwardedPorts = [],
    micAllowed = false,
    arcEnabled = false,
  } = optional;
  crostiniPage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
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

/**
 * @param {!Element} select
 * @param {number} index
 */
function selectContainerByIndex(select, index) {
  assertTrue(!!select);
  const mdSelect =
      select.root.querySelector('select#selectContainer.md-select');
  assertTrue(!!mdSelect);
  mdSelect.selectedIndex = index;
  mdSelect.dispatchEvent(new CustomEvent('change'));
  flush();
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    crostiniPage.showCrostini = true;
    crostiniPage.allowCrostini = true;
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('<settings-crostini-confirmation-dialog>', function() {
    let dialog;
    let cancelOrCloseEvents;
    let closeEventPromise;

    setup(function() {
      cancelOrCloseEvents = [];
      dialog = document.createElement('settings-crostini-confirmation-dialog');

      dialog.addEventListener('cancel', (e) => {
        cancelOrCloseEvents.push(e);
      });
      closeEventPromise =
          new Promise((resolve) => dialog.addEventListener('close', (e) => {
            cancelOrCloseEvents.push(e);
            resolve();
          }));

      document.body.appendChild(dialog);
    });

    teardown(function() {
      dialog.remove();
    });

    test('accept', async function() {
      assertTrue(dialog.shadowRoot.querySelector('cr-dialog').open);
      dialog.shadowRoot.querySelector('.action-button').click();

      await closeEventPromise;
      assertEquals(cancelOrCloseEvents.length, 1);
      assertEquals(cancelOrCloseEvents[0].type, 'close');
      assertTrue(cancelOrCloseEvents[0].detail.accepted);
      assertFalse(dialog.shadowRoot.querySelector('cr-dialog').open);
    });

    test('cancel', async function() {
      assertTrue(dialog.shadowRoot.querySelector('cr-dialog').open);
      dialog.shadowRoot.querySelector('.cancel-button').click();

      await closeEventPromise;
      assertEquals(cancelOrCloseEvents.length, 2);
      assertEquals(cancelOrCloseEvents[0].type, 'cancel');
      assertEquals(cancelOrCloseEvents[1].type, 'close');
      assertFalse(cancelOrCloseEvents[1].detail.accepted);
      assertFalse(dialog.shadowRoot.querySelector('cr-dialog').open);
    });
  });

  suite('MainPage', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('NotSupported', function() {
      crostiniPage.showCrostini = false;
      crostiniPage.allowCrostini = false;
      flush();
      assertTrue(!!crostiniPage.shadowRoot.querySelector('#enable'));
      assertFalse(
          !!crostiniPage.shadowRoot.querySelector('cr-policy-indicator'));
    });

    test('NotAllowed', function() {
      crostiniPage.showCrostini = true;
      crostiniPage.allowCrostini = false;
      flush();
      assertTrue(!!crostiniPage.shadowRoot.querySelector('#enable'));
      assertTrue(
          !!crostiniPage.shadowRoot.querySelector('cr-policy-indicator'));
    });

    test('Enable', function() {
      const button = crostiniPage.shadowRoot.querySelector('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.shadowRoot.querySelector('.subpage-arrow'));
      assertFalse(button.disabled);

      button.click();
      flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.shadowRoot.querySelector('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', async function() {
      const button = crostiniPage.shadowRoot.querySelector('#enable');
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

    test('Deep link to setup Crostini', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '800');
      Router.getInstance().navigateTo(
          routes.CROSTINI, params);

      const deepLinkElement = crostiniPage.shadowRoot.querySelector('#enable');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Enable Crostini button should be focused for settingId=800.');
    });
  });

  suite('SubPageDetails', function() {
    /** @type {?SettingsCrostiniSubPageElement} */
    let subpage;

    setup(async function() {
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
      crostiniPage.shadowRoot.querySelector('#crostini').click();

      await flushTasks();
      subpage =
          crostiniPage.shadowRoot.querySelector('settings-crostini-subpage');
      assertTrue(!!subpage);
    });

    suite('SubPageDefault', function() {
      test('Basic', function() {
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-shared-paths'));
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-shared-usb-devices'));
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-export-import'));
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-enable-arc-adb'));
        assertTrue(!!subpage.shadowRoot.querySelector('#remove'));
        assertTrue(!!subpage.shadowRoot.querySelector('#container-upgrade'));
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-port-forwarding'));
        assertTrue(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-toggle'));
        assertTrue(!!subpage.shadowRoot.querySelector('#crostini-disk-resize'));
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-extra-containers'));
      });

      test('SharedPaths', async function() {
        assertTrue(
            !!subpage.shadowRoot.querySelector('#crostini-shared-paths'));
        subpage.shadowRoot.querySelector('#crostini-shared-paths').click();

        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-guest-os-shared-paths');
        assertTrue(!!subpage);
      });

      test('ContainerUpgrade', function() {
        assertTrue(
            !!subpage.shadowRoot.querySelector('#container-upgrade cr-button'));
        subpage.shadowRoot.querySelector('#container-upgrade cr-button')
            .click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniContainerUpgradeView'));
      });

      test('ContainerUpgradeButtonDisabledOnUpgradeDialog', async function() {
        const button =
            subpage.shadowRoot.querySelector('#container-upgrade cr-button');
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

      test('ContainerUpgradeButtonDisabledOnInstall', async function() {
        const button =
            subpage.shadowRoot.querySelector('#container-upgrade cr-button');
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

      test('InstallerStatusQueriedOnAttach', async function() {
        // We navigated the page during setup, so this request should've been
        // triggered by here.
        assertTrue(
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniInstallerStatus') >= 1);
      });

      test('ToggleCrostiniMicPermissionCancel', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-toggle'));
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));

        setCrostiniPrefs(true, {micAllowed: true});
        assertTrue(
            subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
                .checked);

        subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
            .click();
        await flushTasks();
        assertTrue(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));
        const dialog =
            subpage.shadowRoot.querySelector('#crostini-mic-permission-dialog');
        const dialogClosedPromise = eventToPromise('close', dialog);
        dialog.shadowRoot.querySelector('.cancel-button').click();
        await Promise.all([dialogClosedPromise, flushTasks()]);

        // Because the dialog was cancelled, the toggle should not have changed.
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));
        assertTrue(
            subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
                .checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('ToggleCrostiniMicPermissionShutdown', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-toggle'));
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));

        setCrostiniPrefs(true, {micAllowed: false});
        assertFalse(
            subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
                .checked);

        subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
            .click();
        await flushTasks();
        assertTrue(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));
        const dialog =
            subpage.shadowRoot.querySelector('#crostini-mic-permission-dialog');
        const dialogClosedPromise = eventToPromise('close', dialog);
        dialog.shadowRoot.querySelector('.action-button').click();
        await Promise.all([dialogClosedPromise, flushTasks()]);
        assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));
        assertTrue(
            subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
                .checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));

        // Crostini is now shutdown, this means that it doesn't need to be
        // restarted in order for changes to take effect, therefore no dialog is
        // needed and the mic sharing settings can be changed immediately.
        subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
            .click();
        await flushTasks();
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#crostini-mic-permission-dialog'));
        assertFalse(
            subpage.shadowRoot.querySelector('#crostini-mic-permission-toggle')
                .checked);
        assertFalse(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('Remove', async function() {
        assertTrue(!!subpage.shadowRoot.querySelector('#remove cr-button'));
        subpage.shadowRoot.querySelector('#remove cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await eventToPromise('popstate', window);
        assertEquals(Router.getInstance().currentRoute, routes.CROSTINI);
        assertTrue(!!crostiniPage.shadowRoot.querySelector('#enable'));
      });

      test('RemoveHidden', async function() {
        // Elements are not destroyed when a dom-if stops being shown, but we
        // can check if their rendered width is non-zero. This should be
        // resilient against most formatting changes, since we're not relying on
        // them having any exact size, or on Polymer using any particular means
        // of hiding elements.
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        webUIListenerCallback('crostini-installer-status-changed', true);

        await flushTasks();
        assertFalse(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        webUIListenerCallback('crostini-installer-status-changed', false);

        await flushTasks();
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
      });

      test('HideOnDisable', async function() {
        assertEquals(
            Router.getInstance().currentRoute, routes.CROSTINI_DETAILS);
        setCrostiniPrefs(false);

        await eventToPromise('popstate', window);
        assertEquals(Router.getInstance().currentRoute, routes.CROSTINI);
      });

      test('DiskResizeOpensWhenClicked', async function() {
        assertTrue(!!subpage.shadowRoot.querySelector('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});
        subpage.shadowRoot.querySelector('#showDiskResizeButton').click();

        await flushTasks();
        const dialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
      });

      test('Deep link to resize disk', async () => {
        assertTrue(!!subpage.shadowRoot.querySelector('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});

        const params = new URLSearchParams();
        params.append('settingId', '805');
        Router.getInstance().navigateTo(
            routes.CROSTINI_DETAILS, params);

        const deepLinkElement =
            subpage.shadowRoot.querySelector('#showDiskResizeButton');
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            'Resize disk button should be focused for settingId=805.');
      });
    });

    suite('subPageBackupRestore', function() {
      /** @type {?SettingsCrostiniExportImportElement} */
      let subpage;

      setup(async function() {
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
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-export-import');

        assertTrue(!!subpage);
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
        const params = new URLSearchParams();
        params.append('settingId', '802');
        Router.getInstance().navigateTo(routes.CROSTINI_EXPORT_IMPORT, params);

        flush();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-export-import');

        const deepLinkElement =
            subpage.shadowRoot.querySelector('#export cr-button');
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            'Export button should be focused for settingId=802.');
      });

      test('ExportSingleContainer', async function() {
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#exportCrostiniLabel .secondary'));
        assertTrue(!!subpage.shadowRoot.querySelector('#export cr-button'));
        subpage.shadowRoot.querySelector('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });

      test('ExportMultiContainer', async function() {
        crostiniBrowserProxy.containerInfo = multipleContainers;
        webUIListenerCallback('crostini-container-info', multipleContainers);
        await flushTasks();

        assertTrue(!!subpage.shadowRoot.querySelector(
            '#exportCrostiniLabel .secondary'));
        const select = subpage.root.querySelector('#exportContainerSelect');
        selectContainerByIndex(select, 1);

        assertTrue(!!subpage.shadowRoot.querySelector('#export cr-button'));
        subpage.shadowRoot.querySelector('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
        const args = crostiniBrowserProxy.getArgs('exportCrostiniContainer');
        assertEquals(1, args.length);
        assertEquals(args[0].vm_name, 'not-termina');
        assertEquals(args[0].container_name, 'not-penguin');
      });

      test('ImportSingleContainer', async function() {
        assertFalse(!!subpage.shadowRoot.querySelector(
            '#importCrostiniLabel .secondary'));
        subpage.shadowRoot.querySelector('#import cr-button').click();

        await flushTasks();
        subpage = subpage.shadowRoot.querySelector(
            'settings-crostini-import-confirmation-dialog');
        subpage.shadowRoot.querySelector('cr-dialog cr-button[id="continue"]')
            .click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
      });

      test('ImportMultiContainer', async function() {
        crostiniBrowserProxy.containerInfo = multipleContainers;
        webUIListenerCallback('crostini-container-info', multipleContainers);
        await flushTasks();

        assertTrue(!!subpage.shadowRoot.querySelector(
            '#importCrostiniLabel .secondary'));
        const select = subpage.root.querySelector('#importContainerSelect');
        selectContainerByIndex(select, 1);

        assertTrue(!!subpage.shadowRoot.querySelector('#import cr-button'));
        subpage.shadowRoot.querySelector('#import cr-button').click();
        await flushTasks();
        subpage = subpage.shadowRoot.querySelector(
            'settings-crostini-import-confirmation-dialog');
        subpage.shadowRoot.querySelector('cr-dialog cr-button[id="continue"]')
            .click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
        const args = crostiniBrowserProxy.getArgs('importCrostiniContainer');
        assertEquals(1, args.length);
        assertEquals(args[0].vm_name, 'not-termina');
        assertEquals(args[0].container_name, 'not-penguin');
      });

      test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
        assertFalse(
            subpage.shadowRoot.querySelector('#export cr-button').disabled);
        assertFalse(
            subpage.shadowRoot.querySelector('#import cr-button').disabled);
        webUIListenerCallback(
            'crostini-export-import-operation-status-changed', true);

        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-export-import');
        assertTrue(
            subpage.shadowRoot.querySelector('#export cr-button').disabled);
        assertTrue(
            subpage.shadowRoot.querySelector('#import cr-button').disabled);
        webUIListenerCallback(
            'crostini-export-import-operation-status-changed', false);

        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-export-import');
        assertFalse(
            subpage.shadowRoot.querySelector('#export cr-button').disabled);
        assertFalse(
            subpage.shadowRoot.querySelector('#import cr-button').disabled);
      });

      test(
          'ExportImportButtonsDisabledOnWhenInstallingCrostini',
          async function() {
            assertFalse(
                subpage.shadowRoot.querySelector('#export cr-button').disabled);
            assertFalse(
                subpage.shadowRoot.querySelector('#import cr-button').disabled);
            webUIListenerCallback('crostini-installer-status-changed', true);

            await flushTasks();
            subpage = crostiniPage.shadowRoot.querySelector(
                'settings-crostini-export-import');
            assertTrue(
                subpage.shadowRoot.querySelector('#export cr-button').disabled);
            assertTrue(
                subpage.shadowRoot.querySelector('#import cr-button').disabled);
            webUIListenerCallback('crostini-installer-status-changed', false);

            await flushTasks();
            subpage = crostiniPage.shadowRoot.querySelector(
                'settings-crostini-export-import');
            assertFalse(
                subpage.shadowRoot.querySelector('#export cr-button').disabled);
            assertFalse(
                subpage.shadowRoot.querySelector('#import cr-button').disabled);
          });
    });

    suite('SubPagePortForwarding', function() {
      /** @type {?SettingsCrostiniPortForwarding} */
      let subpage;

      const allContainers = /** @type {!Array<!ContainerInfo>}*/
          ([
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
          ]);
      setup(async function() {
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
            },
            {
              port_number: 5001,
              protocol_type: 1,
              label: 'Label2',
              vm_name: 'not-termina',
              container_name: 'not-penguin',
            },
          ],
        });

        await flushTasks();
        Router.getInstance().navigateTo(
            routes.CROSTINI_PORT_FORWARDING);

        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        assertTrue(!!subpage);
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestContainerInfo'));
      });

      test('DisplayPorts', async function() {
        // Extra list item for the titles.
        assertEquals(
            4, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('AddPortSuccess', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        subpage.shadowRoot.querySelector('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.shadowRoot.querySelector(
            'settings-crostini-add-port-dialog');
        const portNumberInput = subpage.root.querySelector('#portNumberInput');
        portNumberInput, focus();
        portNumberInput.value = '5002';
        portNumberInput, blur();
        assertEquals(portNumberInput.invalid, false);
        const portLabelInput = subpage.root.querySelector('#portLabelInput');
        portLabelInput.value = 'Some Label';
        const select =
            subpage.root.querySelector('settings-guest-os-container-select');
        selectContainerByIndex(select, 1);

        subpage.root.querySelector('cr-dialog cr-button[id="continue"]')
            .click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        const args = crostiniBrowserProxy.getArgs('addCrostiniPortForward')[0];
        assertEquals(4, args.length);
        assertEquals(args[0].vm_name, 'not-termina');
        assertEquals(args[0].container_name, 'not-penguin');
      });

      test('AddPortFail', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        subpage.shadowRoot.querySelector('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.shadowRoot.querySelector(
            'settings-crostini-add-port-dialog');
        const portNumberInput = subpage.root.querySelector('#portNumberInput');
        const portLabelInput = subpage.root.querySelector('#portLabelInput');
        const continueButton =
            subpage.root.querySelector('cr-dialog cr-button[id="continue"]');

        assertEquals(portNumberInput.invalid, false);
        portNumberInput.focus();
        portNumberInput.value = '1023';
        continueButton.click();
        assertEquals(
            0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddError'));

        portNumberInput.value = '65536';
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddError'));

        portNumberInput.focus();
        portNumberInput.value = '5000';
        portNumberInput.blur();
        subpage.root.querySelector('cr-dialog cr-button[id="continue"]')
            .click();
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddExisting'));

        portNumberInput.focus();
        portNumberInput.value = '1024';
        portNumberInput.blur();
        assertEquals(continueButton.disabled, false);
        assertEquals(portNumberInput.invalid, false);
        assertEquals(portNumberInput.errorMessage, '');
      });

      test('AddPortCancel', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        subpage.shadowRoot.querySelector('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.shadowRoot.querySelector(
            'settings-crostini-add-port-dialog');
        subpage.root.querySelector('cr-dialog cr-button[id="cancel"]').click();

        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('RemoveAllPorts', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        subpage.shadowRoot.querySelector('#showRemoveAllPortsMenu').click();

        await flushTasks();
        subpage.shadowRoot.querySelector('#removeAllPortsButton').click();
        assertEquals(
            2,
            crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
      });

      test('RemoveSinglePort', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');

        subpage.shadowRoot.querySelector('#removeSinglePortButton0-0').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
        const args =
            crostiniBrowserProxy.getArgs('removeCrostiniPortForward')[0];
        assertEquals(3, args.length);
        assertEquals(args[0].vm_name, 'termina');
        assertEquals(args[0].container_name, 'penguin');
      });


      test('ActivateSinglePortSuccess', async function() {
        assertFalse(subpage.shadowRoot.querySelector('#errorToast').open);
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        const crToggle =
            subpage.shadowRoot.querySelector('#toggleActivationButton0-0');
        assertFalse(crToggle.disabled);
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertFalse(subpage.shadowRoot.querySelector('#errorToast').open);
      });

      test('ActivateSinglePortFail', async function() {
        await flushTasks();
        crostiniBrowserProxy.portOperationSuccess = false;
        assertFalse(subpage.shadowRoot.querySelector('#errorToast').open);
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        const crToggle =
            subpage.shadowRoot.querySelector('#toggleActivationButton1-0');
        assertTrue(!!crToggle);
        assertFalse(crToggle.disabled);
        assertEquals(crToggle.checked, false);
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertEquals(crToggle.checked, false);
        assertTrue(subpage.shadowRoot.querySelector('#errorToast').open);
      });

      test('DeactivateSinglePort', async function() {
        await flushTasks();
        subpage = crostiniPage.shadowRoot.querySelector(
            'settings-crostini-port-forwarding');
        const crToggle =
            subpage.shadowRoot.querySelector('#toggleActivationButton0-0');
        assertFalse(crToggle.disabled);
        crToggle.checked = true;
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('deactivateCrostiniPortForward'));
      });

      test('ActivePortsChanged', async function() {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
          ],
        });
        const crToggle =
            subpage.shadowRoot.querySelector('#toggleActivationButton0-0');

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

      test('PortPrefsChange', async function() {
        // Default prefs should have list items per port, plus one per
        // container.
        assertEquals(
            4, subpage.shadowRoot.querySelectorAll('.list-item').length);

        // When only one the default container has ports, we lose an item for
        // the extra container heading.
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label2',
            },
          ],
        });
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label2',
            },
            {
              port_number: 5002,
              protocol_type: 0,
              label: 'Label3',
            },
          ],
        });
        assertEquals(
            4, subpage.shadowRoot.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {forwardedPorts: []});
        assertEquals(
            0, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('ContainerStopAndStart', async function() {
        const crToggle =
            subpage.shadowRoot.querySelector('#toggleActivationButton0-0');
        assertFalse(crToggle.disabled);

        delete allContainers[0].ipv4;
        webUIListenerCallback(
            'crostini-container-info', structuredClone(allContainers));
        await flushTasks();
        assertTrue(crToggle.disabled);

        allContainers[0].ipv4 = '1.2.3.4';
        webUIListenerCallback(
            'crostini-container-info', structuredClone(allContainers));
        await flushTasks();
        assertFalse(crToggle.disabled);
      });
    });

    suite('DiskResize', async function() {
      let dialog;
      /**
       * Helper function to assert that the expected block is visible and the
       * others are not.
       * @param {!string} selector
       */
      function assertVisibleBlockIs(selector) {
        const selectors =
            ['#unsupported', '#resize-block', '#error', '#loading'];

        assertTrue(isVisible(dialog.shadowRoot.querySelector(selector)));
        selectors.filter(s => s !== selector).forEach(s => {
          assertFalse(isVisible(dialog.shadowRoot.querySelector(s)));
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
        ticks: ticks,
      };

      const sparseDiskData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: false,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks: ticks,
      };

      async function clickShowDiskResize(userChosen) {
        await crostiniBrowserProxy.resolvePromises('getCrostiniDiskInfo', {
          succeeded: true,
          canResize: true,
          isUserChosenSize: userChosen,
          ticks: ticks,
          defaultIndex: 2,
        });
        subpage.shadowRoot.querySelector('#showDiskResizeButton').click();
        await flushTasks();
        dialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-dialog');

        if (userChosen) {
          // We should be on the loading page but unable to kick off a resize
          // yet.
          assertTrue(!!dialog.shadowRoot.querySelector('#loading'));
          assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        }
      }

      setup(async function() {
        assertTrue(!!subpage.shadowRoot.querySelector('#showDiskResizeButton'));
        const subtext =
            subpage.shadowRoot.querySelector('#diskSizeDescription');
      });

      test('ResizeUnsupported', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: true, canResize: false});
        assertFalse(isVisible(
            subpage.shadowRoot.querySelector('#showDiskResizeButton')));
        assertEquals(
            subpage.shadowRoot.querySelector('#diskSizeDescription').innerText,
            loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext'));
      });

      test('ResizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button =
            subpage.shadowRoot.querySelector('#showDiskResizeButton');
        const subtext =
            subpage.shadowRoot.querySelector('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskResizeShowButton'));
        assertEquals(subtext.innerText, 'label 100');
      });

      test('ReserveSizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        const button =
            subpage.shadowRoot.querySelector('#showDiskResizeButton');
        const subtext =
            subpage.shadowRoot.querySelector('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskReserveSizeButton'));
        assertEquals(
            subtext.innerText,
            loadTimeData.getString(
                'crostiniDiskResizeDynamicallyAllocatedSubtext'));
      });

      test('ResizeRecommendationShownCorrectly', async function() {
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertTrue(
            isVisible(dialog.shadowRoot.querySelector('#recommended-size')));
        assertFalse(isVisible(
            dialog.shadowRoot.querySelector('#recommended-size-warning')));
      });

      test('ResizeRecommendationWarningShownCorrectly', async function() {
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        diskInfo.isLowSpaceAvailable = true;
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertFalse(
            isVisible(dialog.shadowRoot.querySelector('#recommended-size')));
        assertTrue(isVisible(
            dialog.shadowRoot.querySelector('#recommended-size-warning')));
      });

      test('MessageShownIfErrorAndCanRetry', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: false, isUserChosenSize: true});

        // We failed, should have a retry button.
        let button = dialog.shadowRoot.querySelector('#retry');
        assertVisibleBlockIs('#error');
        assertTrue(isVisible(button));
        assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);

        // Back to the loading screen.
        button.click();
        await flushTasks();
        assertVisibleBlockIs('#loading');
        assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);

        // And failure page again.
        await crostiniBrowserProxy.rejectPromises('getCrostiniDiskInfo');
        button = dialog.shadowRoot.querySelector('#retry');
        assertTrue(isVisible(button));
        assertVisibleBlockIs('#error');
        assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);
      });

      test('MessageShownIfCannotResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: false, isUserChosenSize: true});
        assertVisibleBlockIs('#unsupported');
        assertTrue(dialog.shadowRoot.querySelector('#resize').disabled);
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);
      });

      test('ResizePageShownIfCanResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        assertVisibleBlockIs('#resize-block');

        assertEquals(
            ticks[0].label,
            dialog.shadowRoot.querySelector('#label-begin').innerText);
        assertEquals(
            ticks[2].label,
            dialog.shadowRoot.querySelector('#label-end').innerText);
        assertEquals(2, dialog.shadowRoot.querySelector('#diskSlider').value);

        assertFalse(dialog.shadowRoot.querySelector('#resize').disabled);
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);
      });

      test('InProgressResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.shadowRoot.querySelector('#resize');
        button.click();
        await flushTasks();
        assertTrue(button.disabled);
        assertFalse(isVisible(dialog.shadowRoot.querySelector('#done')));
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#resizing')));
        assertFalse(
            isVisible(dialog.shadowRoot.querySelector('#resize-error')));
        assertTrue(dialog.shadowRoot.querySelector('#cancel').disabled);
      });

      test('ErrorResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.shadowRoot.querySelector('#resize');
        button.click();
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', false);
        assertFalse(button.disabled);
        assertFalse(isVisible(dialog.shadowRoot.querySelector('#done')));
        assertFalse(isVisible(dialog.shadowRoot.querySelector('#resizing')));
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#resize-error')));
        assertFalse(dialog.shadowRoot.querySelector('#cancel').disabled);
      });

      test('SuccessResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.shadowRoot.querySelector('#resize');
        button.click();
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', true);
        // Dialog should close itself.
        await eventToPromise('close', dialog);
      });

      test('DiskResizeConfirmationDialogShownAndAccepted', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);
        // Dismiss confirmation.
        let confirmationDialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(
            confirmationDialog.shadowRoot.querySelector('#continue')));
        assertTrue(
            isVisible(confirmationDialog.shadowRoot.querySelector('#cancel')));
        confirmationDialog.shadowRoot.querySelector('#continue').click();
        await eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        dialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#resize')));
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#cancel')));

        // Cancel main resize dialog.
        dialog.shadowRoot.querySelector('#cancel').click();
        await eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        await clickShowDiskResize(false);
        confirmationDialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(
            confirmationDialog.shadowRoot.querySelector('#continue')));
        confirmationDialog.shadowRoot.querySelector('#continue').click();
        await eventToPromise('close', confirmationDialog);

        // Main dialog should show again.
        dialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#resize')));
        assertTrue(isVisible(dialog.shadowRoot.querySelector('#cancel')));
      });

      test('DiskResizeConfirmationDialogShownAndCanceled', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);

        const confirmationDialog = subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(
            confirmationDialog.shadowRoot.querySelector('#continue')));
        assertTrue(
            isVisible(confirmationDialog.shadowRoot.querySelector('#cancel')));
        confirmationDialog.shadowRoot.querySelector('#cancel').click();
        await eventToPromise('close', confirmationDialog);

        assertFalse(!!subpage.shadowRoot.querySelector(
            'settings-crostini-disk-resize-dialog'));
      });
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedPathsTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(
          true, {sharedPaths: {path1: ['termina'], path2: ['some-other-vm']}});

      await flushTasks();
      Router.getInstance().navigateTo(
          routes.CROSTINI_SHARED_PATHS);

      await flushTasks();
      flush();
      subpage = crostiniPage.shadowRoot.querySelector(
          'settings-guest-os-shared-paths');
      assertTrue(!!subpage);
    });

    test('Basic', function() {
      assertEquals(1, subpage.shadowRoot.querySelectorAll('.list-item').length);
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  suite('SubPageSharedUsbDevices', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExtraContainers: false,
      });
      guestOsBrowserProxy.sharedUsbDevices = [
        {
          guid: '0001',
          name: 'usb_dev1',
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
          name: 'usb_dev2',
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
      Router.getInstance().navigateTo(
          routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      subpage = crostiniPage.shadowRoot.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpage);
    });

    test('USB devices are shown', function() {
      const items = subpage.shadowRoot.querySelectorAll('.toggle');
      assertEquals(2, items.length);
      assertTrue(items[0].checked);
      assertFalse(items[1].checked);
    });
  });

  // Functionality is already tested in OSSettingsGuestOsSharedUsbDevicesTest,
  // so just check that we correctly set up the page.
  suite('SubPageSharedUsbDevicesMultiContainer', function() {
    let subpage;

    setup(async function() {
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
      subpage = crostiniPage.shadowRoot.querySelector(
          'settings-crostini-shared-usb-devices');
      assertTrue(!!subpage);
    });

    test('USB devices are shown', async function() {
      const guests = subpage.shadowRoot.querySelectorAll('.usb-list-guest-id');
      assertEquals(2, guests.length);
      assertEquals('penguin', guests[0].innerText);
      assertEquals('not-termina:not-penguin', guests[1].innerText);

      const devices =
          subpage.shadowRoot.querySelectorAll('.usb-list-card-label');
      assertEquals(2, devices.length);
      assertEquals('usb_dev2', devices[0].innerText);
      assertEquals('usb_dev3', devices[1].innerText);
    });
  });

  suite('SubPageArcAdb', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true, {arcEnabled: true});
      loadTimeData.overrideValues({
        arcAdbSideloadingSupported: true,
      });

      await flushTasks();
      Router.getInstance().navigateTo(
          routes.CROSTINI_ANDROID_ADB);

      await flushTasks();
      subpage =
          crostiniPage.shadowRoot.querySelector('settings-crostini-arc-adb');
      assertTrue(!!subpage);
    });

    test('Deep link to enable adb debugging', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '804');
      Router.getInstance().navigateTo(
          routes.CROSTINI_ANDROID_ADB, params);

      flush();

      const deepLinkElement =
          subpage.shadowRoot.querySelector('#arcAdbEnabledButton');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Enable adb debugging button should be focused for settingId=804.');
    });
  });
});

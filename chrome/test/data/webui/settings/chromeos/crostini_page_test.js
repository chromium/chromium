// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestGuestOsBrowserProxy} from './test_guest_os_browser_proxy.js';
import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';
import {GuestOsBrowserProxyImpl, CrostiniBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestGuestOsBrowserProxy} */
let guestOsBrowserProxy = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

const MIC_ALLOWED_PATH = 'prefs.crostini.mic_allowed.value';

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
 * Checks whether a given element is visible to the user.
 * @param {!Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  return !!(element && element.getBoundingClientRect().width > 0);
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.instance_ = guestOsBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
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
      assertTrue(dialog.$$('cr-dialog').open);
      dialog.$$('.action-button').click();

      await closeEventPromise;
      assertEquals(cancelOrCloseEvents.length, 1);
      assertEquals(cancelOrCloseEvents[0].type, 'close');
      assertTrue(cancelOrCloseEvents[0].detail.accepted);
      assertFalse(dialog.$$('cr-dialog').open);
    });

    test('cancel', async function() {
      assertTrue(dialog.$$('cr-dialog').open);
      dialog.$$('.cancel-button').click();

      await closeEventPromise;
      assertEquals(cancelOrCloseEvents.length, 2);
      assertEquals(cancelOrCloseEvents[0].type, 'cancel');
      assertEquals(cancelOrCloseEvents[1].type, 'close');
      assertFalse(cancelOrCloseEvents[1].detail.accepted);
      assertFalse(dialog.$$('cr-dialog').open);
    });
  });

  suite('MainPage', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));
      assertFalse(button.disabled);

      button.click();
      flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', async function() {
      const button = crostiniPage.$$('#enable');
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

      const deepLinkElement = crostiniPage.$$('#enable');
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
      crostiniPage.$$('#crostini').click();

      await flushTasks();
      subpage = crostiniPage.$$('settings-crostini-subpage');
      assertTrue(!!subpage);
    });

    suite('SubPageDefault', function() {
      test('Basic', function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        assertTrue(!!subpage.$$('#crostini-shared-usb-devices'));
        assertTrue(!!subpage.$$('#crostini-export-import'));
        assertTrue(!!subpage.$$('#crostini-enable-arc-adb'));
        assertTrue(!!subpage.$$('#remove'));
        assertTrue(!!subpage.$$('#container-upgrade'));
        assertTrue(!!subpage.$$('#crostini-port-forwarding'));
        assertTrue(!!subpage.$$('#crostini-mic-permission-toggle'));
        assertTrue(!!subpage.$$('#crostini-disk-resize'));
        assertTrue(!!subpage.$$('#crostini-extra-containers'));
      });

      test('SharedPaths', async function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        subpage.$$('#crostini-shared-paths').click();

        await flushTasks();
        subpage = crostiniPage.$$('settings-guest-os-shared-paths');
        assertTrue(!!subpage);
      });

      test('ContainerUpgrade', function() {
        assertTrue(!!subpage.$$('#container-upgrade cr-button'));
        subpage.$$('#container-upgrade cr-button').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniContainerUpgradeView'));
      });

      test('ContainerUpgradeButtonDisabledOnUpgradeDialog', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
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
        const button = subpage.$$('#container-upgrade cr-button');
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

      test('Export', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(!!subpage.$$('#export cr-button'));
        subpage.$$('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });

      test('Deep link to backup linux', async () => {
        const params = new URLSearchParams();
        params.append('settingId', '802');
        Router.getInstance().navigateTo(
            routes.CROSTINI_EXPORT_IMPORT, params);

        flush();
        subpage = crostiniPage.$$('settings-crostini-export-import');

        const deepLinkElement = subpage.$$('#export cr-button');
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            'Export button should be focused for settingId=802.');
      });

      test('Import', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        subpage.$$('#import cr-button').click();

        await flushTasks();
        subpage = subpage.$$('settings-crostini-import-confirmation-dialog');
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
      });

      test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
        webUIListenerCallback(
            'crostini-export-import-operation-status-changed', true);

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(subpage.$$('#export cr-button').disabled);
        assertTrue(subpage.$$('#import cr-button').disabled);
        webUIListenerCallback(
            'crostini-export-import-operation-status-changed', false);

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
      });

      test(
          'ExportImportButtonsDisabledOnWhenInstallingCrostini',
          async function() {
            assertTrue(!!subpage.$$('#crostini-export-import'));
            subpage.$$('#crostini-export-import').click();

            await flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
            webUIListenerCallback('crostini-installer-status-changed', true);

            await flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertTrue(subpage.$$('#export cr-button').disabled);
            assertTrue(subpage.$$('#import cr-button').disabled);
            webUIListenerCallback(
                'crostini-installer-status-changed', false);

            await flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
          });

      test('ToggleCrostiniMicPermissionCancel', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-permission-toggle'));
        assertFalse(!!subpage.$$('#crostini-mic-permission-dialog'));

        setCrostiniPrefs(true, {micAllowed: true});
        assertTrue(subpage.$$('#crostini-mic-permission-toggle').checked);

        subpage.$$('#crostini-mic-permission-toggle').click();
        await flushTasks();
        assertTrue(!!subpage.$$('#crostini-mic-permission-dialog'));
        const dialog = subpage.$$('#crostini-mic-permission-dialog');
        const dialogClosedPromise = eventToPromise('close', dialog);
        dialog.$$('.cancel-button').click();
        await Promise.all([dialogClosedPromise, flushTasks()]);

        // Because the dialog was cancelled, the toggle should not have changed.
        assertFalse(!!subpage.$$('#crostini-mic-permission-dialog'));
        assertTrue(subpage.$$('#crostini-mic-permission-toggle').checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('ToggleCrostiniMicPermissionShutdown', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-permission-toggle'));
        assertFalse(!!subpage.$$('#crostini-mic-permission-dialog'));

        setCrostiniPrefs(true, {micAllowed: false});
        assertFalse(subpage.$$('#crostini-mic-permission-toggle').checked);

        subpage.$$('#crostini-mic-permission-toggle').click();
        await flushTasks();
        assertTrue(!!subpage.$$('#crostini-mic-permission-dialog'));
        const dialog = subpage.$$('#crostini-mic-permission-dialog');
        const dialogClosedPromise = eventToPromise('close', dialog);
        dialog.$$('.action-button').click();
        await Promise.all([dialogClosedPromise, flushTasks()]);
        assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
        assertFalse(!!subpage.$$('#crostini-mic-permission-dialog'));
        assertTrue(subpage.$$('#crostini-mic-permission-toggle').checked);
        assertTrue(crostiniPage.get(MIC_ALLOWED_PATH));

        // Crostini is now shutdown, this means that it doesn't need to be
        // restarted in order for changes to take effect, therefore no dialog is
        // needed and the mic sharing settings can be changed immediately.
        subpage.$$('#crostini-mic-permission-toggle').click();
        await flushTasks();
        assertFalse(!!subpage.$$('#crostini-mic-permission-dialog'));
        assertFalse(subpage.$$('#crostini-mic-permission-toggle').checked);
        assertFalse(crostiniPage.get(MIC_ALLOWED_PATH));
      });

      test('Remove', async function() {
        assertTrue(!!subpage.$$('#remove cr-button'));
        subpage.$$('#remove cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await eventToPromise('popstate', window);
        assertEquals(
            Router.getInstance().getCurrentRoute(),
            routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
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
            Router.getInstance().getCurrentRoute(),
            routes.CROSTINI_DETAILS);
        setCrostiniPrefs(false);

        await eventToPromise('popstate', window);
        assertEquals(
            Router.getInstance().getCurrentRoute(),
            routes.CROSTINI);
      });

      test('DiskResizeOpensWhenClicked', async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});
        subpage.$$('#showDiskResizeButton').click();

        await flushTasks();
        const dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
      });

      test('Deep link to resize disk', async () => {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});

        const params = new URLSearchParams();
        params.append('settingId', '805');
        Router.getInstance().navigateTo(
            routes.CROSTINI_DETAILS, params);

        const deepLinkElement = subpage.$$('#showDiskResizeButton');
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            'Resize disk button should be focused for settingId=805.');
      });
    });

    suite('SubPagePortForwarding', function() {
      /** @type {?SettingsCrostiniPortForwarding} */
      let subpage;
      setup(async function() {
        crostiniBrowserProxy.portOperationSuccess = true;
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 1,
              label: 'Label2',
            },
          ]
        });

        await flushTasks();
        Router.getInstance().navigateTo(
            routes.CROSTINI_PORT_FORWARDING);

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('DisplayPorts', async function() {
        // Extra list item for the titles.
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('AddPortSuccess', async function() {
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        portNumberInput, focus();
        portNumberInput.value = '5002';
        portNumberInput, blur();
        assertEquals(portNumberInput.invalid, false);
        const portLabelInput = subpage.$$('#portLabelInput');
        portLabelInput.value = 'Some Label';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        assertEquals(
            4,
            crostiniBrowserProxy.getArgs('addCrostiniPortForward')[0].length);
      });

      test('AddPortFail', async function() {
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        const portLabelInput = subpage.$$('#portLabelInput');
        const continueButton = subpage.$$('cr-dialog cr-button[id="continue"]');

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
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
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
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        subpage.$$('cr-dialog cr-button[id="cancel"]').click();

        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('RemoveAllPorts', async function() {
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveAllPortsMenu').click();

        await flushTasks();
        subpage.$$('#removeAllPortsButton').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
      });

      test('RemoveSinglePort', async function() {
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveSinglePortMenu0').click();
        await flushTasks();

        subpage.$$('#removeSinglePortButton').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
      });


      test('ActivateSinglePortSucess', async function() {
        assertFalse(subpage.$$('#errorToast').open);
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#toggleActivationButton0').click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertFalse(subpage.$$('#errorToast').open);
      });

      test('ActivateSinglePortFail', async function() {
        await flushTasks();
        crostiniBrowserProxy.portOperationSuccess = false;
        assertFalse(subpage.$$('#errorToast').open);
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton1');
        assertEquals(crToggle.checked, false);
        crToggle.click();

        await flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertEquals(crToggle.checked, false);
        assertTrue(subpage.$$('#errorToast').open);
      });

      test('DeactivateSinglePort', async function() {
        await flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton0');
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
          ]
        });
        const crToggle = subpage.$$('#toggleActivationButton0');

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
              label: 'Label1',
            },
          ]
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
              label: 'Label1',
            },
            {
              port_number: 5002,
              protocol_type: 0,
              label: 'Label1',
            },
          ]
        });
        assertEquals(
            4, subpage.shadowRoot.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {forwardedPorts: []});
        assertEquals(
            0, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('CrostiniStopAndStart', async function() {
        const crToggle = subpage.$$('#toggleActivationButton0');
        assertFalse(crToggle.disabled);

        webUIListenerCallback('crostini-status-changed', false);
        assertTrue(crToggle.disabled);

        webUIListenerCallback('crostini-status-changed', true);
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

        assertTrue(isVisible(dialog.$$(selector)));
        selectors.filter(s => s !== selector).forEach(s => {
          assertFalse(isVisible(dialog.$$(s)));
        });
      }

      const ticks = [
        {label: 'label 0', value: 0, ariaLabel: 'label 0'},
        {label: 'label 10', value: 10, ariaLabel: 'label 10'},
        {label: 'label 100', value: 100, ariaLabel: 'label 100'}
      ];

      const resizeableData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: true,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks: ticks
      };

      const sparseDiskData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: false,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks: ticks
      };

      async function clickShowDiskResize(userChosen) {
        await crostiniBrowserProxy.resolvePromises('getCrostiniDiskInfo', {
          succeeded: true,
          canResize: true,
          isUserChosenSize: userChosen,
          ticks: ticks,
          defaultIndex: 2
        });
        subpage.$$('#showDiskResizeButton').click();
        await flushTasks();
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');

        if (userChosen) {
          // We should be on the loading page but unable to kick off a resize
          // yet.
          assertTrue(!!dialog.$$('#loading'));
          assertTrue(dialog.$$('#resize').disabled);
        }
      }

      setup(async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        const subtext = subpage.$$('#diskSizeDescription');
      });

      test('ResizeUnsupported', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: true, canResize: false});
        assertFalse(isVisible(subpage.$$('#showDiskResizeButton')));
        assertEquals(
            subpage.$$('#diskSizeDescription').innerText,
            loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext'));
      });

      test('ResizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskResizeShowButton'));
        assertEquals(subtext.innerText, 'label 100');
      });

      test('ReserveSizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

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

        assertTrue(isVisible(dialog.$$('#recommended-size')));
        assertFalse(isVisible(dialog.$$('#recommended-size-warning')));
      });

      test('ResizeRecommendationWarningShownCorrectly', async function() {
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        diskInfo.isLowSpaceAvailable = true;
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertFalse(isVisible(dialog.$$('#recommended-size')));
        assertTrue(isVisible(dialog.$$('#recommended-size-warning')));
      });

      test('MessageShownIfErrorAndCanRetry', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: false, isUserChosenSize: true});

        // We failed, should have a retry button.
        let button = dialog.$$('#retry');
        assertVisibleBlockIs('#error');
        assertTrue(isVisible(button));
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // Back to the loading screen.
        button.click();
        await flushTasks();
        assertVisibleBlockIs('#loading');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // And failure page again.
        await crostiniBrowserProxy.rejectPromises('getCrostiniDiskInfo');
        button = dialog.$$('#retry');
        assertTrue(isVisible(button));
        assertVisibleBlockIs('#error');
        assertTrue(dialog.$$('#resize').disabled);
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('MessageShownIfCannotResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: false, isUserChosenSize: true});
        assertVisibleBlockIs('#unsupported');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('ResizePageShownIfCanResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        assertVisibleBlockIs('#resize-block');

        assertEquals(ticks[0].label, dialog.$$('#label-begin').innerText);
        assertEquals(ticks[2].label, dialog.$$('#label-end').innerText);
        assertEquals(2, dialog.$$('#diskSlider').value);

        assertFalse(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('InProgressResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        await flushTasks();
        assertTrue(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertTrue(isVisible(dialog.$$('#resizing')));
        assertFalse(isVisible(dialog.$$('#resize-error')));
        assertTrue(dialog.$$('#cancel').disabled);
      });

      test('ErrorResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', false);
        assertFalse(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertFalse(isVisible(dialog.$$('#resizing')));
        assertTrue(isVisible(dialog.$$('#resize-error')));
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('SuccessResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
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
        let confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#continue').click();
        await eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));

        // Cancel main resize dialog.
        dialog.$$('#cancel').click();
        await eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        await clickShowDiskResize(false);
        confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        confirmationDialog.$$('#continue').click();
        await eventToPromise('close', confirmationDialog);

        // Main dialog should show again.
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));
      });

      test('DiskResizeConfirmationDialogShownAndCanceled', async function() {
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);

        const confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#cancel').click();
        await eventToPromise('close', confirmationDialog);

        assertFalse(!!subpage.$$('settings-crostini-disk-resize-dialog'));
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
      subpage = crostiniPage.$$('settings-guest-os-shared-paths');
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
      guestOsBrowserProxy.sharedUsbDevices = [
        {
          guid: '0001',
          name: 'usb_dev1',
          sharedWith: 'termina',
          promptBeforeSharing: false
        },
        {
          guid: '0002',
          name: 'usb_dev2',
          sharedWith: null,
          promptBeforeSharing: false
        },
      ];

      await flushTasks();
      Router.getInstance().navigateTo(
          routes.CROSTINI_SHARED_USB_DEVICES);

      await flushTasks();
      subpage = crostiniPage.$$('settings-guest-os-shared-usb-devices');
      assertTrue(!!subpage);
    });

    test('USB devices are shown', function() {
      const items = subpage.shadowRoot.querySelectorAll('.toggle');
      assertEquals(2, items.length);
      assertTrue(items[0].checked);
      assertFalse(items[1].checked);
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
      subpage = crostiniPage.$$('settings-crostini-arc-adb');
      assertTrue(!!subpage);
    });

    test('Deep link to enable adb debugging', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '804');
      Router.getInstance().navigateTo(
          routes.CROSTINI_ANDROID_ADB, params);

      flush();

      const deepLinkElement = subpage.$$('#arcAdbEnabledButton');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Enable adb debugging button should be focused for settingId=804.');
    });
  });
});

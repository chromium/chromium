// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestGuestOsBrowserProxy} from './test_guest_os_browser_proxy.m.js';
// #import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// #import {GuestOsBrowserProxyImpl, CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestGuestOsBrowserProxy} */
let guestOsBrowserProxy = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

function setCrostiniPrefs(enabled, optional = {}) {
  const {
    sharedPaths = {},
    forwardedPorts = [],
    crostiniMicSharingEnabled = false,
    arcEnabled = false,
  } = optional;
  crostiniPage.prefs = {
    arc: {
      enabled: {value: arcEnabled},
    },
    crostini: {
      enabled: {value: enabled},
      port_forwarding: {ports: {value: forwardedPorts}},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  crostiniBrowserProxy.crostiniMicSharingEnabled = crostiniMicSharingEnabled;
  Polymer.dom.flush();
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
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    settings.GuestOsBrowserProxyImpl.instance_ = guestOsBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
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
      Polymer.dom.flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', async function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);

      await test_util.flushTasks();
      assertFalse(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', true);

      await test_util.flushTasks();
      assertTrue(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', false);

      await test_util.flushTasks();
      assertFalse(button.disabled);
    });

    test('Deep link to setup Crostini', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

      const params = new URLSearchParams;
      params.append('settingId', '800');
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI, params);

      const deepLinkElement = crostiniPage.$$('#enable');
      await test_util.waitAfterNextRender(deepLinkElement);
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
      });

      settings.Router.getInstance().navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();

      await test_util.flushTasks();
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
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertTrue(!!subpage.$$('#crostini-disk-resize'));
      });

      test('SharedPaths', async function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        subpage.$$('#crostini-shared-paths').click();

        await test_util.flushTasks();
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

        await test_util.flushTasks();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', true);

        await test_util.flushTasks();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', false);

        await test_util.flushTasks();
        assertFalse(button.disabled);
      });

      test('ContainerUpgradeButtonDisabledOnInstall', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
        assertTrue(!!button);

        await test_util.flushTasks();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await test_util.flushTasks();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await test_util.flushTasks();
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

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(!!subpage.$$('#export cr-button'));
        subpage.$$('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });

      test('Deep link to backup linux', async () => {
        loadTimeData.overrideValues({isDeepLinkingEnabled: true});
        assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

        const params = new URLSearchParams;
        params.append('settingId', '802');
        settings.Router.getInstance().navigateTo(
            settings.routes.CROSTINI_EXPORT_IMPORT, params);

        Polymer.dom.flush();
        subpage = crostiniPage.$$('settings-crostini-export-import');

        const deepLinkElement = subpage.$$('#export cr-button');
        await test_util.waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            'Export button should be focused for settingId=802.');
      });

      test('Import', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        subpage.$$('#import cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-import-confirmation-dialog');
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
      });

      test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', true);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(subpage.$$('#export cr-button').disabled);
        assertTrue(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', false);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
      });

      test(
          'ExportImportButtonsDisabledOnWhenInstallingCrostini',
          async function() {
            assertTrue(!!subpage.$$('#crostini-export-import'));
            subpage.$$('#crostini-export-import').click();

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback('crostini-installer-status-changed', true);

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertTrue(subpage.$$('#export cr-button').disabled);
            assertTrue(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback(
                'crostini-installer-status-changed', false);

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
          });

      test('ToggleCrostiniMicSharingCancel', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));

        setCrostiniPrefs(true, {crostiniMicSharingEnabled: true});
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);

        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        assertTrue(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        const dialog = subpage.$$('settings-crostini-mic-sharing-dialog');
        const dialogClosedPromise = test_util.eventToPromise('close', dialog);
        dialog.$$('#cancel').click();
        await Promise.all([dialogClosedPromise, test_util.flushTasks()]);

        // Because the dialog was cancelled, the toggle should not have changed.
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);
      });

      test('ToggleCrostiniMicSharingShutdown', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));

        setCrostiniPrefs(true, {crostiniMicSharingEnabled: false});
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(subpage.$$('#crostini-mic-sharing-toggle').checked);

        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        assertTrue(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        const dialog = subpage.$$('settings-crostini-mic-sharing-dialog');
        const dialogClosedPromise = test_util.eventToPromise('close', dialog);
        dialog.$$('#shutdown').click();
        await Promise.all([dialogClosedPromise, test_util.flushTasks()]);
        assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('setCrostiniMicSharingEnabled'));
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);

        // Crostini is now shutdown, this means that it doesn't need to be
        // restarted in order for changes to take effect, therefore no dialog is
        // needed and the mic sharing settings can be changed immediately.
        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertFalse(subpage.$$('#crostini-mic-sharing-toggle').checked);
      });

      test('Remove', async function() {
        assertTrue(!!subpage.$$('#remove cr-button'));
        subpage.$$('#remove cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });

      test('RemoveHidden', async function() {
        // Elements are not destroyed when a dom-if stops being shown, but we
        // can check if their rendered width is non-zero. This should be
        // resilient against most formatting changes, since we're not relying on
        // them having any exact size, or on Polymer using any particular means
        // of hiding elements.
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await test_util.flushTasks();
        assertFalse(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await test_util.flushTasks();
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
      });

      test('HideOnDisable', async function() {
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI_DETAILS);
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
      });

      test('DiskResizeOpensWhenClicked', async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});
        subpage.$$('#showDiskResizeButton').click();

        await test_util.flushTasks();
        const dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
      });

      test('Deep link to resize disk', async () => {
        loadTimeData.overrideValues({isDeepLinkingEnabled: true});
        assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});

        const params = new URLSearchParams;
        params.append('settingId', '805');
        settings.Router.getInstance().navigateTo(
            settings.routes.CROSTINI_DETAILS, params);

        const deepLinkElement = subpage.$$('#showDiskResizeButton');
        await test_util.waitAfterNextRender(deepLinkElement);
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

        await test_util.flushTasks();
        settings.Router.getInstance().navigateTo(
            settings.routes.CROSTINI_PORT_FORWARDING);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('DisplayPorts', async function() {
        // Extra list item for the titles.
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('AddPortSuccess', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
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
      });

      test('AddPortFail', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
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
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        subpage.$$('cr-dialog cr-button[id="cancel"]').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('RemoveAllPorts', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveAllPortsMenu').click();

        await test_util.flushTasks();
        subpage.$$('#removeAllPortsButton').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
      });

      test('RemoveSinglePort', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveSinglePortMenu0').click();
        await test_util.flushTasks();

        subpage.$$('#removeSinglePortButton').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
      });


      test('ActivateSinglePortSucess', async function() {
        assertFalse(subpage.$$('#errorToast').open);
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#toggleActivationButton0').click();

        await test_util.flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertFalse(subpage.$$('#errorToast').open);
      });

      test('ActivateSinglePortFail', async function() {
        await test_util.flushTasks();
        crostiniBrowserProxy.portOperationSuccess = false;
        assertFalse(subpage.$$('#errorToast').open);
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton1');
        assertEquals(crToggle.checked, false);
        crToggle.click();

        await test_util.flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertEquals(crToggle.checked, false);
        assertTrue(subpage.$$('#errorToast').open);
      });

      test('DeactivateSinglePort', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton0');
        crToggle.checked = true;
        crToggle.click();

        await test_util.flushTasks();
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

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed',
            [{'port_number': 5000, 'protocol_type': 0}]);
        await test_util.flushTasks();
        assertTrue(crToggle.checked);

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed', []);
        await test_util.flushTasks();
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

        cr.webUIListenerCallback('crostini-status-changed', false);
        assertTrue(crToggle.disabled);

        cr.webUIListenerCallback('crostini-status-changed', true);
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
        await test_util.flushTasks();
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
        await test_util.flushTasks();
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
        await test_util.flushTasks();
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
        await test_util.eventToPromise('close', dialog);
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
        await test_util.eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));

        // Cancel main resize dialog.
        dialog.$$('#cancel').click();
        await test_util.eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        await clickShowDiskResize(false);
        confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        confirmationDialog.$$('#continue').click();
        await test_util.eventToPromise('close', confirmationDialog);

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
        await test_util.eventToPromise('close', confirmationDialog);

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

      await test_util.flushTasks();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_PATHS);

      await test_util.flushTasks();
      Polymer.dom.flush();
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

      await test_util.flushTasks();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_USB_DEVICES);

      await test_util.flushTasks();
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

      await test_util.flushTasks();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_ANDROID_ADB);

      await test_util.flushTasks();
      subpage = crostiniPage.$$('settings-crostini-arc-adb');
      assertTrue(!!subpage);
    });

    test('Deep link to enable adb debugging', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

      const params = new URLSearchParams;
      params.append('settingId', '804');
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_ANDROID_ADB, params);

      Polymer.dom.flush();

      const deepLinkElement = subpage.$$('#arcAdbEnabledButton');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Enable adb debugging button should be focused for settingId=804.');
    });
  });
});

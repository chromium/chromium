// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniBrowserProxyImpl, CrostiniPortSetting, GuestOsBrowserProxyImpl, SettingsCrostiniDiskResizeDialogElement, SettingsCrostiniSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSliderElement, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGE, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';
import {clearBody} from '../utils.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

suite('<settings-crostini-subpage>', () => {
  let subpage: SettingsCrostiniSubpageElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;
  let crostiniBrowserProxy: TestCrostiniBrowserProxy;

  const MIC_ALLOWED_PREF_PATH = 'prefs.crostini.mic_allowed.value';

  function setCrostiniPrefs(enabled: boolean, {
    sharedPaths = {},
    forwardedPorts = [],
    micAllowed = false,
    arcEnabled = false,
    bruschettaInstalled = false,
  }: PrefParams = {}): void {
    subpage.prefs = {
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

  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
      showCrostiniExportImport: true,
      showCrostiniContainerUpgrade: true,
      showCrostiniPortForwarding: true,
      showCrostiniDiskResize: true,
      arcAdbSideloadingSupported: true,
      showCrostiniExtraContainers: true,
    });

    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    Router.getInstance().navigateTo(routes.CROSTINI_DETAILS);

    clearBody();
    subpage = document.createElement('settings-crostini-subpage');
    document.body.appendChild(subpage);
    setCrostiniPrefs(true, {arcEnabled: true});
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  suite('Subpage default', () => {
    test('Basic', () => {
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniSharedPathsRow')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniSharedUsbDevicesRow')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniExportImportRow')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniEnableArcAdbRow')));
      assertTrue(isVisible(subpage.shadowRoot!.querySelector('#remove')));
      assertTrue(
          isVisible(subpage.shadowRoot!.querySelector('#container-upgrade')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniPortForwardingRow')));
      assertTrue(isVisible(subpage.shadowRoot!.querySelector(
          '#crostini-mic-permission-toggle')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniDiskResizeRow')));
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#crostiniExtraContainersRow')));
    });

    test('Shared paths', async () => {
      const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#crostiniSharedPathsRow');
      assertTrue(!!button);
      button.click();
      flush();

      assertEquals(
          routes.CROSTINI_SHARED_PATHS, Router.getInstance().currentRoute);
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
      let dialog =
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog');
      assertNull(dialog);

      setCrostiniPrefs(true, {micAllowed: true});
      assertTrue(toggle.checked);

      toggle.click();
      await flushTasks();

      dialog =
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog');
      assertTrue(!!dialog);
      const dialogClosedPromise = eventToPromise('close', dialog);
      const cancelBtn =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancelBtn);
      cancelBtn.click();
      await Promise.all([dialogClosedPromise, flushTasks()]);

      // Because the dialog was cancelled, the toggle should not have changed.
      assertNull(
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog'));

      toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
          '#crostini-mic-permission-toggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      assertTrue(subpage.get(MIC_ALLOWED_PREF_PATH));
    });

    test('Toggle crostini mic permission shutdown', async () => {
      // Crostini is assumed to be running when the page is loaded.
      let toggle =
          subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#crostini-mic-permission-toggle');
      assertTrue(!!toggle);
      let dialog =
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog');
      assertNull(dialog);

      setCrostiniPrefs(true, {micAllowed: false});

      assertFalse(toggle.checked);

      toggle.click();
      await flushTasks();
      dialog =
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog');
      assertTrue(!!dialog);
      const dialogClosedPromise = eventToPromise('close', dialog);
      const actionBtn =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!actionBtn);
      actionBtn.click();
      await Promise.all([dialogClosedPromise, flushTasks()]);
      assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
      assertNull(
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog'));
      toggle = subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
          '#crostini-mic-permission-toggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      assertTrue(subpage.get(MIC_ALLOWED_PREF_PATH));

      // Crostini is now shutdown, this means that it doesn't need to be
      // restarted in order for changes to take effect, therefore no dialog is
      // needed and the mic sharing settings can be changed immediately.
      toggle.click();
      await flushTasks();
      assertNull(
          subpage.shadowRoot!.querySelector('#crostini-mic-permission-dialog'));
      assertFalse(toggle.checked);
      assertFalse(subpage.get(MIC_ALLOWED_PREF_PATH));
    });

    // TODO(b/313456787) Re-enable test once fixed.
    /* test('Remove', async () => {
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
    }); */

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

    test('Disabling crostini returns to previous route', async () => {
      assertEquals(routes.CROSTINI_DETAILS, Router.getInstance().currentRoute);
      const popstateEventPromise = eventToPromise('popstate', window);
      setCrostiniPrefs(false);
      await popstateEventPromise;
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
      assertTrue(!!subpage.shadowRoot!.querySelector('#showDiskResizeButton'));
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
          deepLinkElement, subpage.shadowRoot!.activeElement,
          `Resize disk button should be focused for settingId=${
              CROSTINI_DISK_RESIZE_SETTING}.`);
    });
  });

  suite('Disk resize', () => {
    let dialog: SettingsCrostiniDiskResizeDialogElement;
    /**
     * Helper function to assert that the expected block is visible and the
     * others are not.
     */
    function assertVisibleBlockIs(selector: string): void {
      const selectors = ['#unsupported', '#resize-block', '#error', '#loading'];

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
      assertFalse(isVisible(dialog.shadowRoot!.querySelector('#resize-error')));
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
      assertTrue(isVisible(dialog.shadowRoot!.querySelector('#resize-error')));

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
      assertTrue(
          isVisible(confirmationDialog.shadowRoot!.querySelector('#continue')));

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

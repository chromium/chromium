// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsResetBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {LifetimeBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {ESimManagerRemote, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.js';
import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.js';

/** @enum {string} */
const TestNames = {
  PowerwashDialogAction: 'PowerwashDialogAction',
  PowerwashDialogOpenClose: 'PowerwashDialogOpenClose',
  PowerwashFocusDeepLink: 'PowerwashFocusDeepLink',
  PowerwashFocusDeepLinkNoFlag: 'PowerwashFocusDeepLinkNoFlag',
  PowerwashFocusDeepLinkWrongId: 'PowerwashFocusDeepLinkWrongId',
};

suite('DialogTests', function() {
  let resetPage = null;

  /** @type {!settings.ResetPageBrowserProxy} */
  let resetPageBrowserProxy = null;

  /** @type {!LifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  /** @type {!ESimManagerRemote|undefined} */
  let eSimManagerRemote;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    resetPageBrowserProxy = new TestOsResetBrowserProxy();
    OsResetBrowserProxyImpl.setInstanceForTesting(resetPageBrowserProxy);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    PolymerTest.clearBody();
    resetPage = document.createElement('os-settings-reset-page');
    document.body.appendChild(resetPage);
    flush();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
    resetPage.remove();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {function(SettingsPowerwashDialogElement):!Element}
   *     closeButtonFn A function that returns the button to be used for
   *     closing the dialog.
   * @return {!Promise}
   */
  async function testOpenClosePowerwashDialog(closeButtonFn) {
    // Open powerwash dialog.
    assertTrue(!!resetPage);
    resetPage.shadowRoot.querySelector('#powerwash').click();
    await flushAsync();
    const dialog =
        resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
    const onDialogClosed = new Promise(function(resolve, reject) {
      dialog.addEventListener('close', function() {
        assertFalse(dialog.$.dialog.open);
        resolve();
      });
    });

    closeButtonFn(dialog).click();
    return Promise.all([
      onDialogClosed,
      resetPageBrowserProxy.whenCalled('onPowerwashDialogShow'),
    ]);
  }

  async function openDialogWithESimWarning() {
    eSimManagerRemote.addEuiccForTest(2);

    // Set the first profile's state to kActive.
    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    const profile = (await euicc.getProfileList()).profiles[0];
    profile.properties.state = ProfileState.kActive;

    // Click the powerwash button.
    resetPage.shadowRoot.querySelector('#powerwash').click();
    await flushAsync();

    // The eSIM warning should be showing.
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ true);
    const dialog =
        resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
    assertEquals(dialog.shadowRoot.querySelector('iron-list').items.length, 1);

    // The 'Continue' button should initially be disabled.
    assertTrue(dialog.shadowRoot.querySelector('#continue').disabled);
  }

  /**
   * @param {boolean} shouldBeShowingESimWarning
   */
  function assertOpenDialogUIState(shouldBeShowingESimWarning) {
    const dialog =
        resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    assertEquals(
        !!dialog.shadowRoot.querySelector('#powerwashContainer'),
        !shouldBeShowingESimWarning);
    assertEquals(
        !!dialog.shadowRoot.querySelector('#powerwashContainer'),
        !shouldBeShowingESimWarning);
    assertEquals(
        !!dialog.shadowRoot.querySelector('#powerwash'),
        !shouldBeShowingESimWarning);

    assertEquals(
        !!dialog.shadowRoot.querySelector('#profilesListContainer'),
        shouldBeShowingESimWarning);
    assertEquals(
        !!dialog.shadowRoot.querySelector('#continue'),
        shouldBeShowingESimWarning);
  }

  /**
   * Navigates to the deep link provided by |settingId| and returns true if
   * the focused element is |deepLinkElement|.
   * @param {!Element} deepLinkElement
   * @param {!string} settingId
   * @returns {!boolean}
   */
  async function isDeepLinkFocusedForSettingId(deepLinkElement, settingId) {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.OS_RESET, params);

    await waitAfterNextRender(deepLinkElement);
    return deepLinkElement === getDeepActiveElement();
  }

  // Tests that the powerwash dialog with no EUICC opens and closes correctly,
  // and that chrome.send calls are propagated as expected.
  test(TestNames.PowerwashDialogOpenClose, function() {
    // Test case where the 'cancel' button is clicked.
    return testOpenClosePowerwashDialog(function(dialog) {
      return dialog.$.cancel;
    });
  });

  // Tests that when powerwash is requested chrome.send calls are
  // propagated as expected.
  test(TestNames.PowerwashDialogAction, async () => {
    // Open powerwash dialog.
    resetPage.shadowRoot.querySelector('#powerwash').click();
    await flushAsync();
    const dialog =
        resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
    dialog.shadowRoot.querySelector('#powerwash').click();
    const requestTpmFirmwareUpdate =
        await lifetimeBrowserProxy.whenCalled('factoryReset');
    assertFalse(requestTpmFirmwareUpdate);
  });

  // Tests that when the route changes to one containing a deep link to
  // powerwash, powerwash is focused.
  test(TestNames.PowerwashFocusDeepLink, async () => {
    assertTrue(
        await isDeepLinkFocusedForSettingId(
            resetPage.shadowRoot.querySelector('#powerwash'), '1600'),
        'Powerwash should be focused for settingId=1600.');
  });

  // Tests that when the route changes to one containing a deep link not equal
  // to powerwash, no focusing of powerwash occurs.
  test(TestNames.PowerwashFocusDeepLinkWrongId, async () => {
    assertFalse(
        await isDeepLinkFocusedForSettingId(
            resetPage.shadowRoot.querySelector('#powerwash'), '1234'),
        'Powerwash should not be focused for settingId=1234.');
  });

  test(
      'EUICC with no non-pending profiles shows powerwash dialog', async () => {
        eSimManagerRemote.addEuiccForTest(2);

        return testOpenClosePowerwashDialog(function(dialog) {
          return dialog.$.cancel;
        });
      });

  test('Non-pending profile shows eSIM warning dialog', async () => {
    await openDialogWithESimWarning();

    // Clicking the checkbox should enable the 'Continue' button.
    const dialog =
        resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
    const continueButton = dialog.shadowRoot.querySelector('#continue');
    dialog.shadowRoot.querySelector('cr-checkbox').click();
    assertFalse(continueButton.disabled);

    // Click the 'Continue' button.
    continueButton.click();
    await flushAsync();
    // The powerwash UI should now be showing.
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
  });

  test(
      'eSIM warning dialog link click goes to mobile data subpage',
      async () => {
        await openDialogWithESimWarning();

        const dialog =
            resetPage.shadowRoot.querySelector('os-settings-powerwash-dialog');
        const mobileSettingsLink =
            dialog.shadowRoot.querySelector('localized-link')
                .shadowRoot.querySelector('a');
        assertTrue(!!mobileSettingsLink);

        mobileSettingsLink.click();
        await flushAsync();

        assertEquals(
            routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
        assertEquals(
            'type=Cellular',
            Router.getInstance().getQueryParameters().toString());
      });
});

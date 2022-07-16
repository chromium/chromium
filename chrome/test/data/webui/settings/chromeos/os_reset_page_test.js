// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {OsResetBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {LifetimeBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

cr.define('settings_reset_page', function() {
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

    /** @type {!settings.LifetimeBrowserProxy} */
    let lifetimeBrowserProxy = null;

    /** @type {!chromeos.cellularSetup.mojom.ESimManagerRemote|undefined} */
    let eSimManagerRemote;

    setup(function() {
      lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
      settings.LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

      resetPageBrowserProxy = new reset_page.TestOsResetBrowserProxy();
      settings.OsResetBrowserProxyImpl.instance_ = resetPageBrowserProxy;

      eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
      cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

      PolymerTest.clearBody();
      resetPage = document.createElement('os-settings-reset-page');
      document.body.appendChild(resetPage);
      Polymer.dom.flush();
    });

    teardown(function() {
      settings.Router.getInstance().resetRouteForTesting();
      resetPage.remove();
    });

    function flushAsync() {
      Polymer.dom.flush();
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
      resetPage.$$('#powerwash').click();
      await flushAsync();
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
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
      profile.properties.state =
          chromeos.cellularSetup.mojom.ProfileState.kActive;

      // Click the powerwash button.
      resetPage.$$('#powerwash').click();
      await flushAsync();

      // The eSIM warning should be showing.
      assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ true);
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      assertEquals(dialog.$$('iron-list').items.length, 1);

      // The 'Continue' button should initially be disabled.
      assertTrue(dialog.$$('#continue').disabled);
    }

    /**
     * @param {boolean} shouldBeShowingESimWarning
     */
    function assertOpenDialogUIState(shouldBeShowingESimWarning) {
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);

      assertEquals(
          !!dialog.$$('#powerwashContainer'), !shouldBeShowingESimWarning);
      assertEquals(
          !!dialog.$$('#powerwashContainer'), !shouldBeShowingESimWarning);
      assertEquals(!!dialog.$$('#powerwash'), !shouldBeShowingESimWarning);

      assertEquals(
          !!dialog.$$('#profilesListContainer'), shouldBeShowingESimWarning);
      assertEquals(!!dialog.$$('#continue'), shouldBeShowingESimWarning);
    }

    /**
     * Navigates to the deep link provided by |settingId| and returns true if
     * the focused element is |deepLinkElement|.
     * @param {!Element} deepLinkElement
     * @param {!string} settingId
     * @returns {!boolean}
     */
    async function isDeepLinkFocusedForSettingId(deepLinkElement, settingId) {
      const params = new URLSearchParams;
      params.append('settingId', settingId);
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_RESET, params);

      await test_util.waitAfterNextRender(deepLinkElement);
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
      resetPage.$$('#powerwash').click();
      await flushAsync();
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
      dialog.$$('#powerwash').click();
      const requestTpmFirmwareUpdate =
          await lifetimeBrowserProxy.whenCalled('factoryReset');
      assertFalse(requestTpmFirmwareUpdate);
    });

    // Tests that when the route changes to one containing a deep link to
    // powerwash, powerwash is focused.
    test(TestNames.PowerwashFocusDeepLink, async () => {
      assertTrue(
          await isDeepLinkFocusedForSettingId(
              resetPage.$$('#powerwash'), '1600'),
          'Powerwash should be focused for settingId=1600.');
    });

    // Tests that when the route changes to one containing a deep link not equal
    // to powerwash, no focusing of powerwash occurs.
    test(TestNames.PowerwashFocusDeepLinkWrongId, async () => {
      assertFalse(
          await isDeepLinkFocusedForSettingId(
              resetPage.$$('#powerwash'), '1234'),
          'Powerwash should not be focused for settingId=1234.');
    });

    test(
        'EUICC with no non-pending profiles shows powerwash dialog',
        async () => {
          eSimManagerRemote.addEuiccForTest(2);

          return testOpenClosePowerwashDialog(function(dialog) {
            return dialog.$.cancel;
          });
        });

    test('Non-pending profile shows eSIM warning dialog', async () => {
      await openDialogWithESimWarning();

      // Clicking the checkbox should enable the 'Continue' button.
      const dialog = resetPage.$$('os-settings-powerwash-dialog');
      const continueButton = dialog.$$('#continue');
      dialog.$$('cr-checkbox').click();
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

          const dialog = resetPage.$$('os-settings-powerwash-dialog');
          const mobileSettingsLink =
              dialog.$$('localized-link').shadowRoot.querySelector('a');
          assertTrue(!!mobileSettingsLink);

          mobileSettingsLink.click();
          await flushAsync();

          assertEquals(
              settings.routes.INTERNET_NETWORKS,
              settings.Router.getInstance().getCurrentRoute());
          assertEquals(
              'type=Cellular',
              settings.Router.getInstance().getQueryParameters().toString());
        });
  });

  // #cr_define_end
  return {};
});

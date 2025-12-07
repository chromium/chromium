// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
import {TileType} from 'chrome://customize-chrome-side-panel.top-chrome/tile_type.mojom-webui.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('ShortcutsTest', () => {
  let customizeShortcutsElement: ShortcutsElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
  });

  function getCustomLinksButton(): CrRadioButtonElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '[name="customLinksOption"]')!;
  }

  function getTopSitesButton(): CrRadioButtonElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '[name="topSitesOption"]')!;
  }

  function getEnterpriseShortcutsButton(): CrRadioButtonElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '[name="enterpriseShortcutsOption"]')!;
  }

  function getPersonalShortcutsToggle(): CrCheckboxElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#personalToggle')!;
  }

  function getEnterpriseShortcutsToggle(): CrCheckboxElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#enterpriseToggle')!;
  }

  function getCustomLinksContainer(): HTMLElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#customLinksContainer')!;
  }

  function getTopSitesContainer(): HTMLElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#topSitesContainer')!;
  }

  function getEnterpriseShortcutsContainer(): HTMLElement {
    return customizeShortcutsElement.shadowRoot.querySelector(
        '#enterpriseShortcutsContainer')!;
  }

  async function setInitialSettings(
      shortcutsTypes: TileType[], shortcutsVisible: boolean,
      shortcutsPersonalVisible: boolean,
      disabledShortcuts: TileType[]): Promise<void> {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    await handler.whenCalled('updateMostVisitedSettings');
    callbackRouterRemote.setMostVisitedSettings(
        shortcutsTypes, shortcutsVisible, shortcutsPersonalVisible,
        disabledShortcuts);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(getCustomLinksButton().hideLabelText);
    assertTrue(getTopSitesButton().hideLabelText);
    assertEquals(getCustomLinksButton().label, 'My shortcuts');
    assertEquals(getTopSitesButton().label, 'Most visited sites');
    const enterpriseShortcutsMixedContainer =
        customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(
            '#enterpriseShortcutsMixedContainer');
    const personalShortcutsContainer =
        customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(
            '#personalShortcutsContainer');
    if (customizeShortcutsElement['showEnterprisePersonalMixedSidepanel_']()) {
      assertTrue(!!enterpriseShortcutsMixedContainer);
      assertTrue(!!personalShortcutsContainer);
      const enterpriseButtonLabel =
          enterpriseShortcutsMixedContainer.querySelector(
              'customize-chrome-button-label')!;
      assertEquals(enterpriseButtonLabel.label, 'My organization\'s shortcuts');
      const personalButtonLabel = personalShortcutsContainer.querySelector(
          'customize-chrome-button-label')!;
      assertEquals(personalButtonLabel.label, 'My shortcuts and sites');
    } else if (disabledShortcuts.includes(TileType.kEnterpriseShortcuts)) {
      assertEquals(
          customizeShortcutsElement.shadowRoot.querySelector(
              '#enterpriseShortcutsContainer'),
          null);
    } else {
      assertTrue(getEnterpriseShortcutsButton().hideLabelText);
      assertEquals(
          getEnterpriseShortcutsButton().label, 'My organization\'s shortcuts');
    }
  }

  function assertShown(shown: boolean) {
    assertEquals(shown, customizeShortcutsElement.$.showToggle.checked);
  }

  function assertCustomLinksEnabled() {
    assertEquals(
        'customLinksOption',
        customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  function assertUseMostVisited() {
    assertEquals(
        'topSitesOption', customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  function assertNothingSelected() {
    assertEquals(
        undefined, customizeShortcutsElement.$.radioSelection.selected);
    assertShown(true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kCustomLinks],
        /* shortcutsVisible= */ false, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertCustomLinksEnabled();
    getTopSitesButton().click();
    assertUseMostVisited();
    customizeShortcutsElement.$.showToggle.click();
    assertShown(false);
    customizeShortcutsElement.$.showToggle.click();
    assertUseMostVisited();
  });

  test('turning toggle on updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kTopSites],
        /* shortcutsVisible= */ false, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kTopSites]), JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test('turning toggle off hides settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kTopSites],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);

    customizeShortcutsElement.$.showToggle.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(false, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kTopSites]), JSON.stringify(shortcutsTypes));
    assertFalse(shortcutsVisible);
  });

  test('clicking toggle title updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kTopSites],
        /* shortcutsVisible= */ false, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);

    customizeShortcutsElement.$.showToggleContainer.click();
    await microtasksFinished();

    const selector =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse');
    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kTopSites]), JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test('clicking custom links label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kTopSites],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);
    assertUseMostVisited();

    getCustomLinksContainer().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kCustomLinks]),
        JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test('clicking custom button label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kTopSites],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);
    assertUseMostVisited();

    getCustomLinksButton().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kCustomLinks]),
        JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited label updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kCustomLinks],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);
    assertCustomLinksEnabled();

    getTopSitesContainer().click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kTopSites]), JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test('clicking most visited button updates MV settings', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[TileType.kCustomLinks],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[TileType.kEnterpriseShortcuts]);
    assertCustomLinksEnabled();

    getTopSitesButton().click();

    assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
    const [shortcutsTypes, shortcutsVisible] =
        handler.getArgs('setMostVisitedSettings')[0];
    assertEquals(
        JSON.stringify([TileType.kTopSites]), JSON.stringify(shortcutsTypes));
    assertTrue(shortcutsVisible);
  });

  test(
      'clicking enterprise shortcuts label updates MV settings when mixing is disabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: false,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kCustomLinks],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);
        assertCustomLinksEnabled();

        getEnterpriseShortcutsContainer().click();
        await microtasksFinished();

        assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes, shortcutsVisible] =
            handler.getArgs('setMostVisitedSettings')[0];
        assertEquals(
            JSON.stringify([TileType.kEnterpriseShortcuts]),
            JSON.stringify(shortcutsTypes));
        assertTrue(shortcutsVisible);
      });

  test(
      'clicking enterprise shortcuts button updates MV settings when mixing is disabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: false,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kCustomLinks],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);
        assertCustomLinksEnabled();

        getEnterpriseShortcutsButton().click();

        assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes, shortcutsVisible] =
            handler.getArgs('setMostVisitedSettings')[0];
        assertEquals(
            JSON.stringify([TileType.kEnterpriseShortcuts]),
            JSON.stringify(shortcutsTypes));
        assertTrue(shortcutsVisible);
      });

  test(
      'keydown on radio options updates MV settings when mixing is disabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: false,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kTopSites],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);
        assertUseMostVisited();

        keyDownOn(getTopSitesButton(), 0, [], 'ArrowUp');
        await microtasksFinished();

        assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes, shortcutsVisible] =
            handler.getArgs('setMostVisitedSettings')[0];
        assertEquals(
            JSON.stringify([TileType.kCustomLinks]),
            JSON.stringify(shortcutsTypes));
        assertTrue(shortcutsVisible);

        keyDownOn(getCustomLinksButton(), 0, [], 'ArrowUp');
        await microtasksFinished();

        assertEquals(2, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes2, shortcutsVisible2] =
            handler.getArgs('setMostVisitedSettings')[1];
        assertEquals(
            JSON.stringify([TileType.kEnterpriseShortcuts]),
            JSON.stringify(shortcutsTypes2));
        assertTrue(shortcutsVisible2);

        keyDownOn(getEnterpriseShortcutsButton(), 0, [], 'ArrowDown');
        await microtasksFinished();

        assertEquals(3, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes3, shortcutsVisible3] =
            handler.getArgs('setMostVisitedSettings')[2];
        assertEquals(
            JSON.stringify([TileType.kCustomLinks]),
            JSON.stringify(shortcutsTypes3));
        assertTrue(shortcutsVisible3);

        keyDownOn(getCustomLinksButton(), 0, [], 'ArrowDown');
        await microtasksFinished();

        assertEquals(4, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes4, shortcutsVisible4] =
            handler.getArgs('setMostVisitedSettings')[3];
        assertEquals(
            JSON.stringify([TileType.kTopSites]),
            JSON.stringify(shortcutsTypes4));
        assertTrue(shortcutsVisible4);
      });

  test('no radio option selected if shortcuts type is invalid', async () => {
    await setInitialSettings(
        /* shortcutsTypes= */[100 as TileType],
        /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
        /* disabledShortcuts= */[]);
    assertNothingSelected();
  });

  test('only animates after initialization', async () => {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    const crCollapse =
        customizeShortcutsElement.shadowRoot.querySelector('cr-collapse')!;

    // No animation before initialize.
    assertTrue(crCollapse.noAnimation);

    // Initialize.
    callbackRouterRemote.setMostVisitedSettings(
        /*shortcutsTypes=*/[TileType.kCustomLinks],
        /*shortcutsVisible=*/ true, /*shortcutsPersonalVisible=*/ true,
        /*disabledShortcuts=*/[]);
    await callbackRouterRemote.$.flushForTesting();

    // Animation after initialize.
    assertFalse(crCollapse.noAnimation);

    // Update.
    callbackRouterRemote.setMostVisitedSettings(
        /*shortcutsTypes=*/[TileType.kTopSites],
        /*shortcutsVisible=*/ true, /*shortcutsPersonalVisible=*/ true,
        /*disabledShortcuts=*/[]);
    await callbackRouterRemote.$.flushForTesting();

    // Still animation after update.
    assertFalse(crCollapse.noAnimation);
  });

  test(
      'toggling enterprise shortcuts updates MV settings when mixing enabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: true,
        });
        await setInitialSettings(
            /* shortcutsTypes= */
            [TileType.kCustomLinks, TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);
        assertCustomLinksEnabled();

        getEnterpriseShortcutsToggle().click();
        await microtasksFinished();

        assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes, shortcutsVisible, shortcutsPersonalVisible] =
            handler.getArgs('setMostVisitedSettings')[0];
        assertEquals(
            JSON.stringify([TileType.kCustomLinks]),
            JSON.stringify(shortcutsTypes));
        assertTrue(shortcutsVisible);
        assertTrue(shortcutsPersonalVisible);
      });

  test(
      'toggling personal shortcuts updates MV settings when mixing enabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: true,
        });
        await setInitialSettings(
            /* shortcutsTypes= */
            [TileType.kCustomLinks, TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);
        assertCustomLinksEnabled();

        getPersonalShortcutsToggle().click();
        await microtasksFinished();

        assertEquals(1, handler.getCallCount('setMostVisitedSettings'));
        const [shortcutsTypes, shortcutsVisible, shortcutsPersonalVisible] =
            handler.getArgs('setMostVisitedSettings')[0];
        assertEquals(
            JSON.stringify(
                [TileType.kEnterpriseShortcuts, TileType.kCustomLinks]),
            JSON.stringify(shortcutsTypes));
        assertTrue(shortcutsVisible);
        assertFalse(shortcutsPersonalVisible);
      });

  test(
      'radio options enabled when personal shortcuts on and mixing enabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: true,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);

        assertFalse(getCustomLinksButton().disabled);
        assertFalse(getTopSitesButton().disabled);
      });

  test(
      'radio options disabled when personal shortcuts off and mixing enabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: true,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ false,
            /* disabledShortcuts= */[]);
        await microtasksFinished();

        assertTrue(customizeShortcutsElement.$.radioSelection.disabled);
      });

  test(
      'enterprise and personal shortcut containers visible when mixing enabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: true,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);

        const enterpriseShortcutsMixedContainer =
            customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(
                '#enterpriseShortcutsMixedContainer');
        assertTrue(!!enterpriseShortcutsMixedContainer);
        assertFalse(enterpriseShortcutsMixedContainer.hidden);

        const personalShortcutsContainer =
            customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(

                '#personalShortcutsContainer');
        assertTrue(!!personalShortcutsContainer);
        assertFalse(personalShortcutsContainer.hidden);
      });

  test(
      'enterprise and personal shortcut containers not visible when mixing disabled',
      async () => {
        loadTimeData.overrideValues({
          ntpEnterpriseShortcutsMixingAllowed: false,
        });
        await setInitialSettings(
            /* shortcutsTypes= */[TileType.kEnterpriseShortcuts],
            /* shortcutsVisible= */ true, /* shortcutsPersonalVisible= */ true,
            /* disabledShortcuts= */[]);

        const enterpriseShortcutsMixedContainer =
            customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(
                '#enterpriseShortcutsMixedContainer');
        assertTrue(!!enterpriseShortcutsMixedContainer);
        assertTrue(enterpriseShortcutsMixedContainer.hidden);

        const personalShortcutsContainer =
            customizeShortcutsElement.shadowRoot.querySelector<HTMLElement>(
                '#personalShortcutsContainer');
        assertTrue(!!personalShortcutsContainer);
        assertTrue(personalShortcutsContainer.hidden);
      });

  suite('Metrics', () => {
    test('Clicking show shortcuts toggle sets metric', async () => {
      customizeShortcutsElement.$.showToggle.click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED));

      customizeShortcutsElement.$.showToggleContainer.click();
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      assertEquals(
          2, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          2,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SHOW_SHORTCUTS_TOGGLE_CLICKED));
    });
  });
});

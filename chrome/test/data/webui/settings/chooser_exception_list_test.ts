// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ChooserException, ChooserExceptionListElement, RawChooserException, RawSiteException, SiteException} from 'chrome://settings/lazy_load.js';
import {ChooserType, ContentSettingsTypes, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair,createRawChooserException,createRawSiteException,createSiteSettingsPrefs} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for chooser-exception-list. */

/**
 * An example pref that does not contain any entries.
 */
let prefsEmpty: SiteSettingsPref;

/**
 * An example pref with only user granted USB exception.
 */
let prefsUserProvider: SiteSettingsPref;

/**
 * An example pref with only policy granted USB exception.
 */
let prefsPolicyProvider: SiteSettingsPref;

/**
 * An example pref with 3 USB exception items. The first item will have a user
 * granted site exception and a policy granted site exception. The second item
 * will only have a policy granted site exception. The last item will only have
 * a user granted site exception.
 */
let prefsUsb: SiteSettingsPref;

/**
 * Creates all the test
 */
function populateTestExceptions() {
  prefsEmpty = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [] /* chooserExceptionsList */);

  prefsUserProvider = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [createContentSettingTypeToValuePair(ContentSettingsTypes.USB_DEVICES, [
        createRawChooserException(
            ChooserType.USB_DEVICES,
            [createRawSiteException('https://foo.com')]),
      ])] /* chooserExceptionsList */);

  prefsPolicyProvider = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [createContentSettingTypeToValuePair(ContentSettingsTypes.USB_DEVICES, [
        createRawChooserException(
            ChooserType.USB_DEVICES,
            [createRawSiteException(
                'https://foo.com', {source: SiteSettingSource.POLICY})]),
      ])] /* chooserExceptionsList */);

  prefsUsb =
      createSiteSettingsPrefs([] /* defaultsList */, [] /* exceptionsList */, [
        createContentSettingTypeToValuePair(
            ContentSettingsTypes.USB_DEVICES,
            [
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [
                    createRawSiteException(
                        'https://foo-policy.com',
                        {source: SiteSettingSource.POLICY}),
                    createRawSiteException('https://foo-user.com'),
                  ],
                  {
                    displayName: 'Gadget',
                  }),
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [createRawSiteException('https://bar-policy.com', {
                    source: SiteSettingSource.POLICY,
                  })],
                  {
                    displayName: 'Gizmo',
                  }),
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [createRawSiteException('https://baz-user.com')],
                  {displayName: 'Widget'}),
            ]),
      ] /* chooserExceptionsList */);
}

suite('ChooserExceptionList', function() {
  let testElement: ChooserExceptionListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a chooser-exception-list before each test.
  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('chooser-exception-list');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param chooserType The chooser type to set up the element for.
   * @param prefs The prefs to use.
   */
  function setUpChooserType(
      contentType: ContentSettingsTypes, chooserType: ChooserType,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.category = contentType;
    testElement.chooserType = chooserType;
  }

  function assertSiteOriginsEquals(
      site: RawSiteException, actualSite: SiteException) {
    assertEquals(site.origin, actualSite.origin);
    assertEquals(site.embeddingOrigin, actualSite.embeddingOrigin);
  }

  function assertChooserExceptionEquals(
      exception: RawChooserException, actualException: ChooserException) {
    assertEquals(exception.displayName, actualException.displayName);
    assertEquals(exception.chooserType, actualException.chooserType);
    assertDeepEquals(exception.object, actualException.object);

    const sites = exception.sites;
    const actualSites = actualException.sites;
    assertEquals(sites.length, actualSites.length);
    for (let i = 0; i < sites.length; ++i) {
      assertSiteOriginsEquals(sites[i]!, actualSites[i]!);
    }
  }

  test('getChooserExceptionList API used', async function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES, prefsUsb);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    const chooserType =
        await browserProxy.whenCalled('getChooserExceptionList');
    assertEquals(ChooserType.USB_DEVICES, chooserType);

    // Flush the container to ensure that the container is populated.
    flush();

    // Ensure that each chooser exception is rendered with a
    // chooser-exception-list-entry.
    const chooserExceptionListEntries =
        testElement.shadowRoot!.querySelectorAll(
            'chooser-exception-list-entry');
    assertEquals(3, chooserExceptionListEntries.length);
    for (let i = 0; i < chooserExceptionListEntries.length; ++i) {
      assertChooserExceptionEquals(
          prefsUsb.chooserExceptions[ContentSettingsTypes.USB_DEVICES][i]!,
          chooserExceptionListEntries[i]!.exception);
    }

    // The first chooser exception should render two site exceptions with
    // site-list-entry elements.
    const firstSiteListEntries =
        chooserExceptionListEntries[0]!.shadowRoot!.querySelectorAll(
            'site-list-entry');
    assertEquals(2, firstSiteListEntries.length);

    // The second chooser exception should render one site exception with
    // a site-list-entry element.
    const secondSiteListEntries =
        chooserExceptionListEntries[1]!.shadowRoot!.querySelectorAll(
            'site-list-entry');
    assertEquals(1, secondSiteListEntries.length);

    // The last chooser exception should render one site exception with a
    // site-list-entry element.
    const thirdSiteListEntries =
        chooserExceptionListEntries[2]!.shadowRoot!.querySelectorAll(
            'site-list-entry');
    assertEquals(1, thirdSiteListEntries.length);
  });

  test(
      'User granted chooser exceptions should show the reset button',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsUserProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        // Flush the container to ensure that the container is populated.
        flush();

        const chooserExceptionListEntry = testElement.shadowRoot!.querySelector(
            'chooser-exception-list-entry');
        assertTrue(!!chooserExceptionListEntry);

        const siteListEntry =
            chooserExceptionListEntry!.shadowRoot!.querySelector(
                'site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu container is hidden.
        const dotsMenu = siteListEntry!.$.actionMenuButton;
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is not hidden.
        const resetButton = siteListEntry!.$.resetSite;
        assertFalse(resetButton.hidden);

        // Ensure that the policy enforced indicator is hidden.
        const policyIndicator = siteListEntry!.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertFalse(!!policyIndicator);
      });

  test(
      'Policy granted chooser exceptions should show the policy indicator icon',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        // Flush the container to ensure that the container is populated.
        flush();

        const chooserExceptionListEntry = testElement.shadowRoot!.querySelector(
            'chooser-exception-list-entry');
        assertTrue(!!chooserExceptionListEntry);

        const siteListEntry =
            chooserExceptionListEntry!.shadowRoot!.querySelector(
                'site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu container is hidden.
        const dotsMenu = siteListEntry!.$.actionMenuButton;
        assertTrue(!!dotsMenu);
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is hidden.
        const resetButton = siteListEntry!.$.resetSite;
        assertTrue(resetButton.hidden);

        // Ensure that the policy enforced indicator not is hidden.
        const policyIndicator = siteListEntry!.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);
      });

  test(
      'Site exceptions from mixed sources should display properly',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsUsb);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        // Flush the container to ensure that the container is populated.
        flush();

        const chooserExceptionListEntries =
            testElement.shadowRoot!.querySelectorAll(
                'chooser-exception-list-entry');
        assertEquals(3, chooserExceptionListEntries.length);

        // The first chooser exception contains mixed provider site
        // exceptions.
        const siteListEntries =
            chooserExceptionListEntries[0]!.shadowRoot!.querySelectorAll(
                'site-list-entry');
        assertEquals(2, siteListEntries.length);

        // The first site exception is a policy provided exception, so
        // only the policy indicator should be visible;
        const policyProvidedDotsMenu = siteListEntries[0]!.$.actionMenuButton;
        assertTrue(!!policyProvidedDotsMenu);
        assertTrue(policyProvidedDotsMenu.hidden);

        const policyProvidedResetButton = siteListEntries[0]!.$.resetSite;
        assertTrue(!!policyProvidedResetButton);
        assertTrue(policyProvidedResetButton.hidden);

        const policyProvidedPolicyIndicator =
            siteListEntries[0]!.shadowRoot!.querySelector(
                'cr-policy-pref-indicator');
        assertTrue(!!policyProvidedPolicyIndicator);

        // The second site exception is a user provided exception, so only
        // the reset button should be visible.
        const userProvidedDotsMenu = siteListEntries[1]!.$.actionMenuButton;
        assertTrue(!!userProvidedDotsMenu);
        assertTrue(userProvidedDotsMenu.hidden);

        const userProvidedResetButton = siteListEntries[1]!.$.resetSite;
        assertTrue(!!userProvidedResetButton);
        assertFalse(userProvidedResetButton.hidden);

        const userProvidedPolicyIndicator =
            siteListEntries[1]!.shadowRoot!.querySelector(
                'cr-policy-pref-indicator');
        assertFalse(!!userProvidedPolicyIndicator);
      });

  test('Empty list', async function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES, prefsEmpty);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    const chooserType =
        await browserProxy.whenCalled('getChooserExceptionList');
    assertEquals(ChooserType.USB_DEVICES, chooserType);
    assertEquals(0, testElement.chooserExceptions.length);
    const emptyListMessage = testElement.shadowRoot!.querySelector<HTMLElement>(
        '#empty-list-message')!;
    assertFalse(emptyListMessage.hidden);
    assertEquals('No USB devices found', emptyListMessage.textContent!.trim());
  });

  test('resetChooserExceptionForSite API used', async function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
        prefsUserProvider);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    const chooserType =
        await browserProxy.whenCalled('getChooserExceptionList');
    assertEquals(ChooserType.USB_DEVICES, chooserType);
    assertEquals(1, testElement.chooserExceptions.length);

    assertChooserExceptionEquals(
        prefsUserProvider
            .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0]!,
        testElement.chooserExceptions[0]!);

    // Flush the container to ensure that the container is populated.
    flush();

    const chooserExceptionListEntry =
        testElement.shadowRoot!.querySelector('chooser-exception-list-entry');
    assertTrue(!!chooserExceptionListEntry);

    const siteListEntry =
        chooserExceptionListEntry!.shadowRoot!.querySelector('site-list-entry');
    assertTrue(!!siteListEntry);

    // Assert that the action button is hidden.
    const dotsMenu = siteListEntry!.$.actionMenuButton;
    assertTrue(!!dotsMenu);
    assertTrue(dotsMenu.hidden);

    // Assert that the reset button is visible.
    const resetButton = siteListEntry!.$.resetSite;
    assertFalse(resetButton.hidden);

    resetButton.click();
    const args = await browserProxy.whenCalled('resetChooserExceptionForSite');
    assertEquals(ChooserType.USB_DEVICES, args[0]);
    assertEquals('https://foo.com', args[1]);
    assertDeepEquals({}, args[2]);
  });

  test(
      'The show-tooltip event is fired when mouse hovers over policy ' +
          'indicator and the common tooltip is shown',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        assertEquals(1, testElement.chooserExceptions.length);

        assertChooserExceptionEquals(
            prefsPolicyProvider
                .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0]!,
            testElement.chooserExceptions[0]!);

        // Flush the container to ensure that the container is populated.
        flush();

        const chooserExceptionListEntry = testElement.shadowRoot!.querySelector(
            'chooser-exception-list-entry');
        assertTrue(!!chooserExceptionListEntry);

        const siteListEntry =
            chooserExceptionListEntry!.shadowRoot!.querySelector(
                'site-list-entry');
        assertTrue(!!siteListEntry);

        const tooltip = testElement.$.tooltip;
        assertTrue(!!tooltip);

        const innerTooltip = tooltip.$.tooltip;
        assertTrue(!!innerTooltip);

        /**
         * Create an array of test parameters objects. The parameter
         * properties are the following:
         * |text| Tooltip text to display.
         * |el| Event target element.
         * |eventType| The event type to dispatch to |el|.
         * @type {Array<{text: string, el: !Element, eventType: string}>}
         */
        const testsParams = [
          {text: 'a', el: testElement, eventType: 'mouseleave'},
          {text: 'b', el: testElement, eventType: 'click'},
          {text: 'c', el: testElement, eventType: 'blur'},
          {text: 'd', el: tooltip, eventType: 'mouseenter'},
        ];
        for (const params of testsParams) {
          const text = params.text;
          const eventTarget = params.el;

          siteListEntry!.fire('show-tooltip', {target: testElement, text});
          assertFalse(innerTooltip!.hidden);
          assertEquals(text, tooltip.innerHTML.trim());

          eventTarget.dispatchEvent(new MouseEvent(params.eventType));
          await microtasksFinished();
          assertTrue(innerTooltip!.hidden);
        }
      });

  test(
      'The exception list is updated when the prefs are modified',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsUserProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        let chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        assertEquals(1, testElement.chooserExceptions.length);

        assertChooserExceptionEquals(
            prefsUserProvider
                .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0]!,
            testElement.chooserExceptions[0]!);

        browserProxy.resetResolver('getChooserExceptionList');

        // Simulate a change in preferences.
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);

        webUIListenerCallback(
            'contentSettingChooserPermissionChanged',
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES);
        chooserType = await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        assertEquals(1, testElement.chooserExceptions.length);

        assertChooserExceptionEquals(
            prefsPolicyProvider
                .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0]!,
            testElement.chooserExceptions[0]!);
      });

  test(
      'The exception list is updated when incognito status is changed',
      async function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        let chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        // Flush the container to ensure that the container is populated.
        flush();

        let chooserExceptionListEntry = testElement.shadowRoot!.querySelector(
            'chooser-exception-list-entry');
        assertTrue(!!chooserExceptionListEntry);

        const siteListEntry =
            chooserExceptionListEntry!.shadowRoot!.querySelector(
                'site-list-entry');
        assertTrue(!!siteListEntry);
        // Ensure that the incognito tooltip is hidden.
        const incognitoTooltip =
            siteListEntry!.shadowRoot!.querySelector('#incognitoTooltip');
        assertFalse(!!incognitoTooltip);

        // Simulate an incognito session being created.
        browserProxy.resetResolver('getChooserExceptionList');
        browserProxy.setIncognito(true);
        chooserType = await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);
        // Flush the container to ensure that the container is populated.
        flush();

        chooserExceptionListEntry = testElement.shadowRoot!.querySelector(
            'chooser-exception-list-entry');
        assertTrue(!!chooserExceptionListEntry);
        assertTrue(chooserExceptionListEntry!.$.listContainer
                       .querySelector('iron-list')!.items!.some(
                           item => item.incognito));

        const siteListEntries =
            chooserExceptionListEntry!.shadowRoot!.querySelectorAll(
                'site-list-entry');
        assertEquals(2, siteListEntries.length);
        assertTrue(
            Array.from(siteListEntries).some(entry => entry.model.incognito));

        const tooltip = testElement.$.tooltip;
        assertTrue(!!tooltip);
        const innerTooltip = tooltip.shadowRoot!.querySelector('#tooltip');
        assertTrue(!!innerTooltip);
        const text = loadTimeData.getString('incognitoSiteExceptionDesc');
        // This filtered array should be non-empty due to above test that
        // checks for incognito exception.
        Array.from(siteListEntries)
            .filter(entry => entry.model.incognito)
            .forEach(entry => {
              const incognitoTooltip =
                  entry.shadowRoot!.querySelector('#incognitoTooltip');
              // Make sure it is not hidden if it is an incognito
              // exception
              assertTrue(!!incognitoTooltip);
              // Trigger mouse enter and check tooltip text
              incognitoTooltip!.dispatchEvent(new MouseEvent('mouseenter'));
              assertFalse(innerTooltip!.classList.contains('hidden'));
              assertEquals(text, tooltip.innerHTML.trim());
            });
      });

  test('show confirmation dialog on reset settings', async function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
        prefsUserProvider);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    const chooserType =
        await browserProxy.whenCalled('getChooserExceptionList');
    assertEquals(ChooserType.USB_DEVICES, chooserType);
    assertEquals(1, testElement.chooserExceptions.length);

    assertChooserExceptionEquals(
        prefsUserProvider
            .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0]!,
        testElement.chooserExceptions[0]!);

    // Flush the container to ensure that the container is populated.
    flush();

    const chooserExceptionListEntry =
        testElement.shadowRoot!.querySelector('chooser-exception-list-entry');
    assertTrue(!!chooserExceptionListEntry);

    const siteListEntry =
        chooserExceptionListEntry!.shadowRoot!.querySelector('site-list-entry');
    assertTrue(!!siteListEntry);

    // Check both cancelling and accepting the dialog closes it.
    assertFalse(testElement.$.confirmResetSettings.open);
    ['cancel-button', 'action-button'].forEach(buttonType => {
      testElement.shadowRoot!
          .querySelector<HTMLElement>('#resetSettingsButton')!.click();
      assertTrue(testElement.$.confirmResetSettings.open);
      const actionButtonList =
          testElement.shadowRoot!.querySelectorAll<HTMLElement>(
              `#confirmResetSettings .${buttonType}`);
      assertEquals(1, actionButtonList.length);
      actionButtonList[0]!.click();
      assertFalse(testElement.$.confirmResetSettings.open);
    });

    const args = await browserProxy.whenCalled('resetChooserExceptionForSite');
    assertEquals(testElement.chooserExceptions[0]!.chooserType, args[0]);
    assertEquals(testElement.chooserExceptions[0]!.sites[0]!.origin, args[1]);
    assertDeepEquals(testElement.chooserExceptions[0]!.object, args[2]);
  });
});

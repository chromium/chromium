// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for site-permissions-edit-permissions-dialog.
 * */
import 'chrome://extensions/extensions.js';

import {SitePermissionsEditPermissionsDialogElement} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('SitePermissionsEditPermissionsDialog', function() {
  let element: SitePermissionsEditPermissionsDialogElement;
  let delegate: TestService;
  const SiteSet = chrome.developerPrivate.SiteSet;
  const HostAccess = chrome.developerPrivate.HostAccess;

  const extensions = [
    createExtensionInfo({
      id: 'test_1',
      name: 'test_1',
      iconUrl: 'icon_url',
    }),
    createExtensionInfo({
      id: 'test_2',
      name: 'test_2',
      iconUrl: 'icon_url',
    }),
    createExtensionInfo({
      id: 'test_3',
      name: 'test_3',
      iconUrl: 'icon_url',
    }),
  ];

  const matchingExtensionsInfo = [
    {id: 'test_1', siteAccess: HostAccess.ON_CLICK},
    {id: 'test_2', siteAccess: HostAccess.ON_SPECIFIC_SITES},
  ];

  const changeHostAccess =
      (select: HTMLSelectElement,
       access: chrome.developerPrivate.HostAccess) => {
        select.value = access;
        select.dispatchEvent(new CustomEvent('change'));
      };

  setup(function() {
    loadTimeData.overrideValues({'enableUserPermittedSites': true});

    delegate = new TestService();
    delegate.matchingExtensionsInfo = matchingExtensionsInfo;

    setupElement();
  });

  function setupElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element =
        document.createElement('site-permissions-edit-permissions-dialog');
    element.delegate = delegate;
    element.extensions = extensions;
    element.site = 'http://example.com';
    element.originalSiteSet = SiteSet.USER_PERMITTED;
    document.body.appendChild(element);
  }

  test('extra text shown if site matches subdomains', function() {
    assertEquals('http://example.com', element.$.site.innerText);
    assertFalse(isVisible(element.$.includesSubdomains));

    element.site = '*.example.com';
    flush();

    assertEquals('example.com', element.$.site.innerText);
    assertTrue(isVisible(element.$.includesSubdomains));
  });

  test('editing current site set', async function() {
    flush();
    const siteSetRadioGroup =
        element.shadowRoot!.querySelector('cr-radio-group');

    assertTrue(!!siteSetRadioGroup);
    assertEquals(SiteSet.USER_PERMITTED, siteSetRadioGroup.selected);

    const restrictSiteRadioButton =
        element.shadowRoot!.querySelector<HTMLElement>(
            `cr-radio-button[name=${SiteSet.USER_RESTRICTED}]`);
    assertTrue(!!restrictSiteRadioButton);
    restrictSiteRadioButton.click();

    flush();
    assertEquals(SiteSet.USER_RESTRICTED, siteSetRadioGroup.selected);

    const whenClosed = eventToPromise('close', element);
    element.$.submit.click();

    const [siteSet, sites] = await delegate.whenCalled('addUserSpecifiedSites');
    assertEquals(SiteSet.USER_RESTRICTED, siteSet);
    assertDeepEquals([element.site], sites);

    await whenClosed;
    assertFalse(element.$.dialog.open);
  });

  test(
      'list of matching extensions shown when changing options',
      async function() {
        element.site = 'example.com';
        flush();
        const siteSetRadioGroup =
            element.shadowRoot!.querySelector('cr-radio-group');
        assertTrue(!!siteSetRadioGroup);

        let extensionSiteAccessRows =
            element!.shadowRoot!.querySelectorAll<HTMLElement>(
                '.extension-row');
        assertEquals(0, extensionSiteAccessRows.length);

        const extensionSpecifiedRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
        assertTrue(!!extensionSpecifiedRadioButton);
        extensionSpecifiedRadioButton.click();

        const site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('*://example.com/', site);
        flush();

        assertEquals(SiteSet.EXTENSION_SPECIFIED, siteSetRadioGroup.selected);
        extensionSiteAccessRows =
            element!.shadowRoot!.querySelectorAll<HTMLElement>(
                '.extension-row');
        assertEquals(2, extensionSiteAccessRows.length);

        const whenClosed = eventToPromise('close', element);
        element.$.submit.click();
        const [siteSet, sites] =
            await delegate.whenCalled('removeUserSpecifiedSites');
        assertEquals(SiteSet.USER_PERMITTED, siteSet);

        // Since the site being edited is just a host, remove both the http and
        // https url for the host.
        assertDeepEquals(
            [`http://${element.site}`, `https://${element.site}`], sites);

        await whenClosed;
        assertFalse(element.$.dialog.open);
      });

  test(
      'radio buttons not shown for site matching subdomains', async function() {
        flush();
        const extensionSpecifiedRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
        assertTrue(!!extensionSpecifiedRadioButton);
        extensionSpecifiedRadioButton.click();
        const site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('http://example.com/', site);

        flush();
        assertTrue(
            isVisible(element.shadowRoot!.querySelector('cr-radio-group')));

        element.site = '*.etld.com';
        flush();

        assertFalse(
            isVisible(element.shadowRoot!.querySelector('cr-radio-group')));
      });

  test(
      'list of extensions changes in response to extensions updating',
      async function() {
        flush();
        const extensionSpecifiedRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
        assertTrue(!!extensionSpecifiedRadioButton);
        extensionSpecifiedRadioButton.click();
        let site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('http://example.com/', site);

        flush();

        let extensionSiteAccessSelects =
            element.shadowRoot!.querySelectorAll('select');
        assertEquals(2, extensionSiteAccessSelects.length);
        assertEquals(HostAccess.ON_CLICK, extensionSiteAccessSelects[0]!.value);

        delegate.matchingExtensionsInfo = [
          {id: 'test_1', siteAccess: HostAccess.ON_ALL_SITES},
          {id: 'test_2', siteAccess: HostAccess.ON_SPECIFIC_SITES},
        ];

        element.extensions = [
          createExtensionInfo({
            id: 'test_1',
            name: 'test_1',
            iconUrl: 'icon_url',
          }),
          createExtensionInfo({
            id: 'test_2',
            name: 'test_2',
            iconUrl: 'icon_url',
          }),
        ];

        // Test that changing `element.extensions` causes a call to
        // getMatchingExtensionsForSite.
        site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('http://example.com/', site);
        flush();

        extensionSiteAccessSelects =
            element.shadowRoot!.querySelectorAll('select');
        assertEquals(2, extensionSiteAccessSelects.length);

        // Test that the value displayed for the first extension matches the
        // updated matchingExtensionsInfo.
        assertEquals(
            HostAccess.ON_ALL_SITES, extensionSiteAccessSelects[0]!.value);
      });

  test('editing extension site access', async function() {
    element.site = 'example.com';
    delegate.matchingExtensionsInfo = [
      ...matchingExtensionsInfo,
      {id: 'test_3', siteAccess: HostAccess.ON_ALL_SITES},
    ];

    flush();
    const extensionSpecifiedRadioButton =
        element.shadowRoot!.querySelector<HTMLElement>(
            `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
    assertTrue(!!extensionSpecifiedRadioButton);
    extensionSpecifiedRadioButton.click();

    const site = await delegate.whenCalled('getMatchingExtensionsForSite');
    assertEquals('*://example.com/', site);
    flush();

    const extensionSiteAccessRows =
        element.shadowRoot!.querySelectorAll<HTMLElement>('.extension-row');
    assertEquals(3, extensionSiteAccessRows.length);

    const siteAccessSelectMenus =
        element.shadowRoot!.querySelectorAll<HTMLSelectElement>(
            '.extension-host-access');
    assertEquals(3, siteAccessSelectMenus.length);

    // Edit the site access values for the first two extensions.
    changeHostAccess(siteAccessSelectMenus[0]!, HostAccess.ON_SPECIFIC_SITES);
    changeHostAccess(siteAccessSelectMenus[1]!, HostAccess.ON_ALL_SITES);

    // Edit the site access for the third extension once, then change it
    // back to the original value.
    changeHostAccess(siteAccessSelectMenus[2]!, HostAccess.ON_CLICK);
    changeHostAccess(siteAccessSelectMenus[2]!, HostAccess.ON_ALL_SITES);

    const whenClosed = eventToPromise('close', element);
    element.$.submit.click();
    await delegate.whenCalled('removeUserSpecifiedSites');

    const [siteToUpdate, siteAccessUpdates] =
        await delegate.whenCalled('updateSiteAccess');
    // For updating the extensions' site access, check that a wildcard
    // host is used if the site was a host only.
    assertEquals('*://example.com/', siteToUpdate);

    // Since the site access for extension "test_3" was ultimately not
    // changed through the select menu, it should not be included in
    // `siteAccessUpdates`.
    assertDeepEquals(
        [
          {id: 'test_1', siteAccess: HostAccess.ON_SPECIFIC_SITES},
          {id: 'test_2', siteAccess: HostAccess.ON_ALL_SITES},
        ],
        siteAccessUpdates);

    await whenClosed;
    assertFalse(element.$.dialog.open);
  });

  test(
      'updateSiteAccess arguments are updated in response to extension updates',
      async function() {
        element.site = 'http://example.com';
        delegate.matchingExtensionsInfo = [
          ...matchingExtensionsInfo,
          {id: 'test_3', siteAccess: HostAccess.ON_ALL_SITES},
        ];

        flush();
        const extensionSpecifiedRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
        assertTrue(!!extensionSpecifiedRadioButton);
        extensionSpecifiedRadioButton.click();

        let site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('http://example.com/', site);
        flush();

        const siteAccessSelectMenus =
            element.shadowRoot!.querySelectorAll<HTMLSelectElement>(
                '.extension-host-access');
        assertEquals(3, siteAccessSelectMenus.length);

        // Edit the site access values for all three extensions.
        changeHostAccess(
            siteAccessSelectMenus[0]!, HostAccess.ON_SPECIFIC_SITES);
        changeHostAccess(siteAccessSelectMenus[1]!, HostAccess.ON_ALL_SITES);
        changeHostAccess(siteAccessSelectMenus[2]!, HostAccess.ON_CLICK);

        // Simulate an update event happening. Note that the new site access for
        // `test_1` is now the same as what was edited and `test_3` no longer
        // exists.
        delegate.matchingExtensionsInfo = [
          {id: 'test_1', siteAccess: HostAccess.ON_SPECIFIC_SITES},
          {id: 'test_2', siteAccess: HostAccess.ON_SPECIFIC_SITES},
        ];

        element.extensions = [
          createExtensionInfo({
            id: 'test_1',
            name: 'test_1',
            iconUrl: 'icon_url',
          }),
          createExtensionInfo({
            id: 'test_2',
            name: 'test_2',
            iconUrl: 'icon_url',
          }),
        ];

        // Changing `element.extensions` causes a call to
        // getMatchingExtensionsForSite.
        site = await delegate.whenCalled('getMatchingExtensionsForSite');
        assertEquals('http://example.com/', site);
        flush();

        const whenClosed = eventToPromise('close', element);
        element.$.submit.click();
        await delegate.whenCalled('removeUserSpecifiedSites');

        const [siteToUpdate, siteAccessUpdates] =
            await delegate.whenCalled('updateSiteAccess');
        assertEquals('http://example.com/', siteToUpdate);

        // Only the site access update for `test_2` should be included, as after
        // the update, there's no change for site access for `test_1` and
        // `test_3` no longer exists.
        assertDeepEquals(
            [{id: 'test_2', siteAccess: HostAccess.ON_ALL_SITES}],
            siteAccessUpdates);

        await whenClosed;
        assertFalse(element.$.dialog.open);
      });

  test(
      'permitted sites not visible when enableUserPermittedSites flag is false',
      function() {
        loadTimeData.overrideValues({'enableUserPermittedSites': false});

        // set up the element again to capture the updated value of
        // enableUserPermittedSites.
        setupElement();

        flush();

        // Only the user restricted and extension specified radio buttons should
        // be visible.
        const permittedSiteRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.USER_PERMITTED}]`);
        assertFalse(isVisible(permittedSiteRadioButton));

        const restrictedSiteRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.USER_RESTRICTED}]`);
        assertTrue(isVisible(restrictedSiteRadioButton));

        const extensionSiteRadioButton =
            element.shadowRoot!.querySelector<HTMLElement>(
                `cr-radio-button[name=${SiteSet.EXTENSION_SPECIFIED}]`);
        assertTrue(isVisible(extensionSiteRadioButton));
      });
});

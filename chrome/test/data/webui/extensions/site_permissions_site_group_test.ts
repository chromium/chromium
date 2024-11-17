// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-permissions-site-group. */
import 'chrome://extensions/extensions.js';

import type {SitePermissionsSiteGroupElement} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SitePermissionsSiteGroupElement', function() {
  const PERMITTED_TEXT = loadTimeData.getString('permittedSites');
  const RESTRICTED_TEXT = loadTimeData.getString('restrictedSites');

  let element: SitePermissionsSiteGroupElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('site-permissions-site-group');
    document.body.appendChild(element);
  });

  test('clicking expand shows all sites within this group', async function() {
    element.data = {
      etldPlusOne: 'google.ca',
      numExtensions: 0,
      sites: [
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
          numExtensions: 0,
          site: 'images.google.ca',
        },
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
          numExtensions: 0,
          site: 'google.ca',
        },
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
          numExtensions: 0,
          site: '*.google.ca',
        },
      ],
    };
    await microtasksFinished();

    assertEquals('google.ca', element.$.etldOrSite.innerText);
    assertEquals(PERMITTED_TEXT, element.$.etldOrSiteSubtext.innerText);

    const sitesList =
        element.shadowRoot!.querySelector<HTMLElement>('#sites-list');
    assertFalse(isVisible(sitesList));
    const expandButton = element.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(isVisible(sitesList));
    const expandedSites =
        element.shadowRoot!.querySelectorAll<HTMLElement>('#sites-list .site');
    const expandedIncludesSubdomains =
        element.shadowRoot!.querySelectorAll<HTMLElement>(
            '#sites-list .includes-subdomains');

    assertEquals('images.google.ca', expandedSites[0]!.innerText);
    assertFalse(isVisible(expandedIncludesSubdomains[0]!));
    assertEquals('google.ca', expandedSites[1]!.innerText);
    assertFalse(isVisible(expandedIncludesSubdomains[1]!));

    // The site shown should not have the subdomain specifier '*.'.
    assertEquals('google.ca', expandedSites[2]!.innerText);
    // But there should be text indicating that it includes subdomains.
    assertTrue(isVisible(expandedIncludesSubdomains[2]!));
  });

  test('no subtext shown for sites from different sets', async function() {
    element.data = {
      etldPlusOne: 'google.ca',
      numExtensions: 0,
      sites: [
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
          numExtensions: 0,
          site: 'images.google.ca',
        },
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_RESTRICTED,
          numExtensions: 0,
          site: 'google.ca',
        },
      ],
    };
    await microtasksFinished();

    assertEquals('google.ca', element.$.etldOrSite.innerText);
    assertEquals('', element.$.etldOrSiteSubtext.innerText);

    const expandButton = element.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(isVisible(
        element.shadowRoot!.querySelector<HTMLElement>('#sites-list')));
    const expandedSites = element.shadowRoot!.querySelectorAll<HTMLElement>(
        '#sites-list .site-subtext');

    // The subtext for each expanded site should show which set it's from.
    assertEquals(PERMITTED_TEXT, expandedSites[0]!.innerText);
    assertEquals(RESTRICTED_TEXT, expandedSites[1]!.innerText);
  });

  test('full site shown if there is only one site in group', async function() {
    element.data = {
      etldPlusOne: 'example.com',
      numExtensions: 0,
      sites: [{
        siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
        numExtensions: 0,
        site: 'a.example.com',
      }],
    };
    await microtasksFinished();

    assertEquals('a.example.com', element.$.etldOrSite.innerText);
    assertFalse(isVisible(element.$.etldOrSiteIncludesSubdomains));
    assertEquals(PERMITTED_TEXT, element.$.etldOrSiteSubtext.innerText);

    assertFalse(isVisible(
        element.shadowRoot!.querySelector<HTMLElement>('cr-expand-button')));

    // Now set the element's one site in the group to match subdomains.
    element.data = {
      etldPlusOne: 'example.com',
      numExtensions: 1,
      sites: [{
        siteSet: chrome.developerPrivate.SiteSet.EXTENSION_SPECIFIED,
        numExtensions: 1,
        site: '*.example.com',
      }],
    };
    await microtasksFinished();

    assertEquals('example.com', element.$.etldOrSite.innerText);
    assertTrue(isVisible(element.$.etldOrSiteIncludesSubdomains));
    assertEquals(
        loadTimeData.getString('sitePermissionsAllSitesOneExtension'),
        element.$.etldOrSiteSubtext.innerText);
  });

  test(
      'clicking the arrow for a single site shows dialog for that site',
      async function() {
        element.data = {
          etldPlusOne: 'example.com',
          numExtensions: 0,
          sites: [{
            siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
            numExtensions: 0,
            site: 'a.example.com',
          }],
        };
        await microtasksFinished();

        const editSiteButton = element.shadowRoot!.querySelector<HTMLElement>(
            '#edit-one-site-button');
        assertTrue(isVisible(editSiteButton));

        editSiteButton!.click();
        await microtasksFinished();

        const dialog = element.shadowRoot!.querySelector(
            'site-permissions-edit-permissions-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('a.example.com', dialog.site);
        assertEquals(
            chrome.developerPrivate.SiteSet.USER_PERMITTED,
            dialog.originalSiteSet);
      });

  test(
      'clicking the arrow for an expanded site shows dialog for that site',
      async function() {
        element.data = {
          etldPlusOne: 'google.ca',
          numExtensions: 0,
          sites: [
            {
              siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
              numExtensions: 0,
              site: 'images.google.ca',
            },
            {
              siteSet: chrome.developerPrivate.SiteSet.USER_RESTRICTED,
              numExtensions: 0,
              site: 'google.ca',
            },
          ],
        };
        await microtasksFinished();

        element.shadowRoot!.querySelector<HTMLElement>(
                               'cr-expand-button')!.click();
        await microtasksFinished();

        const editSiteButtons =
            element.shadowRoot!.querySelectorAll<HTMLElement>('cr-icon-button');
        assertEquals(2, editSiteButtons.length);

        editSiteButtons[1]!.click();
        await microtasksFinished();

        const dialog = element.shadowRoot!.querySelector(
            'site-permissions-edit-permissions-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('google.ca', dialog.site);
        assertEquals(
            chrome.developerPrivate.SiteSet.USER_RESTRICTED,
            dialog.originalSiteSet);
      });
});

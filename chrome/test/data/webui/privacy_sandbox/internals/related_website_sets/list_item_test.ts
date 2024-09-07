// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSet, RelatedWebsiteSetsListItemElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {SAMPLE_RELATED_WEBSITE_SET, SAMPLE_RELATED_WEBSITE_SET_MANAGED_BY_ENTERPRISE} from './test_data.js';

suite('ListItemTest', () => {
  let item: RelatedWebsiteSetsListItemElement;
  const sampleSet: RelatedWebsiteSet = SAMPLE_RELATED_WEBSITE_SET;
  const sampleManagedByEnterpriseSet: RelatedWebsiteSet =
      SAMPLE_RELATED_WEBSITE_SET_MANAGED_BY_ENTERPRISE;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('related-website-sets-list-item');
    document.body.appendChild(item);
    item.primarySite = sampleSet.primarySite;
    item.memberSites = sampleSet.memberSites;
    item.managedByEnterprise = sampleSet.managedByEnterprise;
    await microtasksFinished();
  });

  test('check layout', async () => {
    assertTrue(isVisible(item));
    assertFalse(item.$.expandedContent.opened);
    const enterpriseIcon = item.shadowRoot!.querySelector('cr-icon');
    assertFalse(isVisible(enterpriseIcon));
  });

  test('check layout with enterprise icon', async () => {
    item.primarySite = sampleManagedByEnterpriseSet.primarySite;
    item.memberSites = sampleManagedByEnterpriseSet.memberSites;
    item.managedByEnterprise = sampleManagedByEnterpriseSet.managedByEnterprise;
    await microtasksFinished();

    const enterpriseIcon = item.shadowRoot!.querySelector('cr-icon');
    assertTrue(isVisible(item));
    assertFalse(item.$.expandedContent.opened);
    assertTrue(isVisible(enterpriseIcon));
  });

  test('check expansion', async () => {
    item.$.expandButton.click();
    await microtasksFinished();

    assertTrue(item.$.expandedContent.opened);
    const memberSites = Array.from(item.$.expandedContent.children);
    assertEquals(sampleSet.memberSites.length, memberSites.length);

    memberSites.forEach(member => assertTrue(isVisible(member)));
  });

  test('check bold with primary site', async () => {
    item.query = 'primary';
    await microtasksFinished();

    const boldedText = item.shadowRoot!.querySelector('b');
    assertEquals('primary', boldedText!.textContent!.trim());
  });

  test('check bold with member site', async () => {
    item.query = 'associated1';
    await microtasksFinished();

    const boldedText = item.shadowRoot!.querySelector('b');
    assertEquals('associated1', boldedText!.textContent!.trim());
  });

  test('check issuer text on search', async () => {
    item.expanded = false;
    item.primarySite = 'seT3';
    item.memberSites = [];
    item.managedByEnterprise = false;
    item.query = 'sEt3';
    await microtasksFinished();
    const primarySite = item.shadowRoot!.querySelector<HTMLElement>('b');
    assertTrue(!!primarySite);
    assertTrue(
        primarySite.innerText.toLowerCase().includes(item.query.toLowerCase()));
  });
});

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {createDestinationWithCertificateStatus} from './print_preview_test_utils.js';

window.destination_item_test = {};
const destination_item_test = window.destination_item_test;
destination_item_test.suiteName = 'DestinationItemTest';
/** @enum {string} */
destination_item_test.TestNames = {
  Online: 'online',
  Offline: 'offline',
  BadCertificate: 'bad certificate',
  QueryName: 'query name',
  QueryDescription: 'query description',
};

suite(destination_item_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationListItemElement} */
  let item;

  /** @type {string} */
  const printerId = 'FooDevice';

  /** @type {string} */
  const printerName = 'FooName';

  /** @override */
  setup(function() {
    document.body.innerHTML = '';
    item = /** @type {!PrintPreviewDestinationListItemElement} */ (
        document.createElement('print-preview-destination-list-item'));

    // Create destination
    item.destination = new Destination(
        printerId, DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        printerName, DestinationConnectionStatus.ONLINE);
    item.searchQuery = null;
    document.body.appendChild(item);
  });

  // Test that the destination is displayed correctly for the basic case of an
  // online destination with no search query.
  test(assert(destination_item_test.TestNames.Online), function() {
    const name = item.$$('.name');
    assertEquals(printerName, name.textContent);
    assertEquals('1', window.getComputedStyle(name).opacity);
    assertEquals('', item.$$('.search-hint').textContent.trim());
    assertEquals('', item.$$('.connection-status').textContent.trim());
    assertTrue(item.$$('.learn-more-link').hidden);
    assertTrue(item.$$('.extension-controlled-indicator').hidden);
  });

  // Test that the destination is opaque and the correct status shows up if
  // the destination is stale.
  test(assert(destination_item_test.TestNames.Offline), function() {
    const now = new Date();
    const twoMonthsAgo = new Date(now.getTime());
    let month = twoMonthsAgo.getMonth() - 2;
    if (month < 0) {
      month = month + 12;
      twoMonthsAgo.setFullYear(now.getFullYear() - 1);
    }
    twoMonthsAgo.setMonth(month);
    item.destination = new Destination(
        printerId, DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        printerName, DestinationConnectionStatus.OFFLINE,
        {lastAccessTime: twoMonthsAgo.getTime()});

    const name = item.$$('.name');
    assertEquals(printerName, name.textContent);
    assertEquals('0.4', window.getComputedStyle(name).opacity);
    assertEquals('', item.$$('.search-hint').textContent.trim());
    assertEquals(
        loadTimeData.getString('offlineForMonth'),
        item.$$('.connection-status').textContent.trim());
    assertTrue(item.$$('.learn-more-link').hidden);
    assertTrue(item.$$('.extension-controlled-indicator').hidden);
  });

  // Test that the destination is opaque and the correct status shows up if
  // the destination has a bad cloud print certificate.
  test(assert(destination_item_test.TestNames.BadCertificate), function() {
    loadTimeData.overrideValues({isEnterpriseManaged: false});
    item.destination =
        createDestinationWithCertificateStatus(printerId, printerName, true);

    const name = item.$$('.name');
    assertEquals(printerName, name.textContent);
    assertEquals('0.4', window.getComputedStyle(name).opacity);
    assertEquals('', item.$$('.search-hint').textContent.trim());
    assertEquals(
        loadTimeData.getString('noLongerSupported'),
        item.$$('.connection-status').textContent.trim());
    assertFalse(item.$$('.learn-more-link').hidden);
    assertTrue(item.$$('.extension-controlled-indicator').hidden);
  });

  // Test that the destination is displayed correctly when the search query
  // matches its display name.
  test(assert(destination_item_test.TestNames.QueryName), function() {
    item.searchQuery = /(Foo)/ig;

    const name = item.$$('.name');
    assertEquals(printerName + printerName, name.textContent);

    // Name should be highlighted.
    const searchHits = name.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('Foo', searchHits[0].textContent);

    // No hints.
    assertEquals('', item.$$('.search-hint').textContent.trim());
  });

  // Test that the destination is displayed correctly when the search query
  // matches its description.
  test(assert(destination_item_test.TestNames.QueryDescription), function() {
    const params = {
      description: 'ABCPrinterBrand Model 123',
      location: 'Building 789 Floor 6',
    };
    item.destination = new Destination(
        printerId, DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        printerName, DestinationConnectionStatus.ONLINE, params);
    item.searchQuery = /(ABC)/ig;

    // No highlighting on name.
    const name = item.$$('.name');
    assertEquals(printerName, name.textContent);
    assertEquals(0, name.querySelectorAll('.search-highlight-hit').length);

    // Search hint should be have the description and be highlighted.
    const hint = item.$$('.search-hint');
    assertTrue(hint.textContent.includes(params.description));
    assertFalse(hint.textContent.includes(params.location));
    const searchHits = hint.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('ABC', searchHits[0].textContent);
  });
});

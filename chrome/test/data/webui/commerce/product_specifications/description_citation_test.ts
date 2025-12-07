// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/description_citation.js';

import type {DescriptionCitationElement} from 'chrome://compare/description_citation.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('DescriptionCitationTest', () => {
  let citationElement: DescriptionCitationElement;
  let metrics: MetricsTracker;
  const mockOpenWindowProxy = TestMock.fromClass(OpenWindowProxyImpl);

  setup(async () => {
    OpenWindowProxyImpl.setInstance(mockOpenWindowProxy);
    metrics = fakeMetricsPrivate();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    citationElement = document.createElement('description-citation');
    citationElement.urlInfo = {
      title: '',
      url: {url: 'http://example.com/initial_url'},
      faviconUrl: {url: ''},
      thumbnailUrl: {url: ''},
      previewText: '',
    };
    document.body.appendChild(citationElement);

    loadTimeData.overrideValues({
      citationA11yLabel: 'Citation $1 of $2, $3, $4. $5. $6.',
    });

    await flushTasks();
  });

  test('citation renders correctly', async () => {
    citationElement.index = 4;
    citationElement.urlInfo = {
      title: 'Page Title',
      url: {url: 'http://example.com/long/path/url'},
      faviconUrl: {url: 'https://example.com/favicon.png'},
      thumbnailUrl: {url: ''},
      previewText: 'Preview text',
    };
    citationElement.citationCount = 8;
    citationElement.productName = 'product';
    await flushTasks();

    assertTrue(!!citationElement.$.citation);
    assertEquals('4', citationElement.$.citation.textContent?.trim());
    assertTrue(citationElement.$.tooltip.textContent?.includes('example.com'));

    assertTrue(!!citationElement.shadowRoot.querySelector('.faviconContainer'));
    assertTrue(!!citationElement.shadowRoot.querySelector('.previewText'));

    const label = loadTimeData.getStringF(
        'citationA11yLabel', 4, 8, 'product', 'example.com', 'Page Title',
        'Preview text');
    const elementAriaContent =
        citationElement.$.citation.getAttribute('aria-label') || '';
    assertEquals(label, elementAriaContent);
  });

  test('citation without favicon url or preview text', async () => {
    citationElement.index = 4;
    citationElement.urlInfo = {
      title: '',
      url: {url: 'http://example.com/long/path/url'},
      faviconUrl: {url: ''},
      thumbnailUrl: {url: ''},
      previewText: '',
    };
    citationElement.citationCount = 8;
    citationElement.productName = 'product';
    await flushTasks();

    assertTrue(!!citationElement.$.citation);
    assertEquals('4', citationElement.$.citation.textContent?.trim());
    assertTrue(citationElement.$.tooltip.textContent?.includes('example.com'));

    assertFalse(
        !!citationElement.shadowRoot.querySelector('.faviconContainer'));
    assertFalse(!!citationElement.shadowRoot.querySelector('.previewText'));

    const expectedAriaLabel = loadTimeData.getStringF(
        'citationA11yLabel', 4, 8, 'product', 'example.com', '', '');
    assertEquals(
        expectedAriaLabel,
        citationElement.$.citation.getAttribute('aria-label'));
  });

  test('tooltip excludes www', async () => {
    citationElement.index = 1;
    citationElement.urlInfo = {
      title: '',
      url: {url: 'http://www.example.com/'},
      faviconUrl: {url: ''},
      thumbnailUrl: {url: ''},
      previewText: '',
    };
    citationElement.citationCount = 1;
    citationElement.productName = 'product';
    await flushTasks();

    assertTrue(!!citationElement.$.citation);
    assertEquals('1', citationElement.$.citation.textContent?.trim());

    // Ensure the citation only contains TLD+1.
    assertTrue(citationElement.$.tooltip.textContent?.includes('example.com'));
    assertFalse(
        citationElement.$.tooltip.textContent?.includes('www.example.com'));
  });

  test('click opens tab', async () => {
    const url = 'http://example.com/long/path/url';
    citationElement.index = 1;
    citationElement.urlInfo = {
      title: '',
      url: {url: url},
      faviconUrl: {url: ''},
      thumbnailUrl: {url: ''},
      previewText: '',
    };
    citationElement.citationCount = 1;
    citationElement.productName = 'product';
    await flushTasks();

    citationElement.$.citation.click();

    const arg = await mockOpenWindowProxy.whenCalled('openUrl');
    assertEquals(url, arg);
    assertEquals(1, metrics.count('Commerce.Compare.CitationClicked'));
  });

  test('tooltip has no animation delay', () => {
    assertEquals(0, citationElement.$.tooltip.animationDelay);
  });
});

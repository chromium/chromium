// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/description_section.js';

import type {DescriptionSectionElement, ProductDescription} from 'chrome://compare/description_section.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DescriptionSectionTest', () => {
  let descriptionSectionElement: DescriptionSectionElement;
  const description: ProductDescription = {
    attributes: [
      {
        label: 'attribute1',
        value: 'value1',
      },
      {
        label: 'attribute2',
        value: 'value2',
      },
    ],
    summary: [
      {
        text: 'summary1',
        urls: [
          {
            url: {url: 'http://example.com/citation1'},
            title: '',
            faviconUrl: {url: ''},
            thumbnailUrl: {url: ''},
            previewText: '',
          },
        ],
      },
      {
        text: 'summary2',
        urls: [
          {
            url: {url: 'http://example.com/citation2'},
            title: '',
            faviconUrl: {url: ''},
            thumbnailUrl: {url: ''},
            previewText: '',
          },
          {
            url: {url: 'http://example.com/citation3'},
            title: '',
            faviconUrl: {url: ''},
            thumbnailUrl: {url: ''},
            previewText: '',
          },
        ],
      },
      {
        text: 'summary3',
        urls: [],
      },
    ],
  };

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    descriptionSectionElement = document.createElement('description-section');
    descriptionSectionElement.description = description;
    descriptionSectionElement.productName = 'product';
    document.body.appendChild(descriptionSectionElement);
    loadTimeData.overrideValues({
      citationA11yLabel: 'Citation $1 of $2, $3, $4',
    });
    await microtasksFinished();
  });

  test('summaries render correctly', () => {
    const summaries =
        descriptionSectionElement.shadowRoot.querySelectorAll('.summary-text');

    assertEquals(description.summary.length, summaries.length);
    summaries.forEach((_item, index) => {
      assertTrue(!!summaries[index]!.textContent);
      assertEquals(
          description.summary[index]!.text,
          summaries[index]!.textContent.trim());
    });
  });

  test('citations are listed correctly', () => {
    const citations = descriptionSectionElement.shadowRoot.querySelectorAll(
        'description-citation');

    assertEquals(3, citations.length);
    assertEquals('1', citations[0]!.getAttribute('index'));
    assertEquals('2', citations[1]!.getAttribute('index'));
    assertEquals('3', citations[2]!.getAttribute('index'));

    assertEquals(
        'Citation 1 of 3, product, example.com',
        citations[0]!.$.citation.getAttribute('aria-label'));
    assertEquals(
        'Citation 2 of 3, product, example.com',
        citations[1]!.$.citation.getAttribute('aria-label'));
    assertEquals(
        'Citation 3 of 3, product, example.com',
        citations[2]!.$.citation.getAttribute('aria-label'));
  });

  test('attributes are listed correctly', () => {
    const attributes = descriptionSectionElement.shadowRoot.querySelectorAll(
        '.attribute-chip');

    assertEquals(description.attributes.length, attributes.length);
    attributes.forEach((attrElement, attrIndex) => {
      assertTrue(!!attrElement.textContent);
      assertTrue(!!description.attributes[attrIndex]);
      assertTrue(attrElement.textContent.trim().includes(
          description.attributes[attrIndex].label));
      assertTrue(attrElement.textContent.trim().includes(
          description.attributes[attrIndex].value));
    });
  });

  test('empty section shown for empty summary', async () => {
    descriptionSectionElement.description = {
      attributes: description.attributes,
      summary: [],
    };
    await microtasksFinished();

    const emptySection = $$(descriptionSectionElement, 'empty-section');
    assertTrue(!!emptySection);
  });

  test('empty section shown for N/A summary texts', async () => {
    descriptionSectionElement.description = {
      attributes: description.attributes,
      summary: [{
        text: 'N/A',
        urls: [],
      }],
    };
    await microtasksFinished();

    const emptySection = $$(descriptionSectionElement, 'empty-section');
    assertTrue(!!emptySection);
  });
});

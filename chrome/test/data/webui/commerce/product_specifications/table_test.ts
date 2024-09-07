// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/table.js';

import type {TableElement} from 'chrome://compare/table.js';
import {WindowProxy} from 'chrome://compare/window_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle, installMock} from './test_support.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;
  let windowProxy: TestMock<WindowProxy>;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  setup(async () => {
    shoppingServiceApi.reset();
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);
    tableElement = document.createElement('product-specifications-table');
    document.body.appendChild(tableElement);
  });

  test('column count correct', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem:
            {title: 'title', url: 'https://example.com/1', imageUrl: ''},
        productDetails: [],

      },
      {
        selectedItem:
            {title: 'title2', url: 'https://example.com/2', imageUrl: ''},
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const columns = tableElement.shadowRoot!.querySelectorAll('.col');
    assertEquals(2, columns.length);
    assertEquals('2', tableElement.style.getPropertyValue('--num-columns'));

    const detailContainers =
        tableElement.shadowRoot!.querySelectorAll('.detail-container');
    assertEquals(0, detailContainers.length);
  });

  test('images are displayed', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem: {
          title: 'item1',
          url: 'https://example.com/1',
          imageUrl: 'https://foo.com/image',
        },
        productDetails: [],
      },
      {
        selectedItem: {
          title: 'item2',
          url: 'https://example.com/2',
          imageUrl: 'https://bar.com/image',
        },
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const images =
        tableElement.shadowRoot!.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(2, images.length);
    assertEquals(
        tableElement.columns[0]!.selectedItem.imageUrl, images[0]!.autoSrc);
    assertEquals(
        tableElement.columns[1]!.selectedItem.imageUrl, images[1]!.autoSrc);

    // Ensure the favicon isn't showing.
    const faviconMainImage = $$<HTMLElement>(tableElement, '.favicon');
    assertFalse(!!faviconMainImage);
  });

  test('fallback images are displayed', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem: {
          title: 'item1',
          url: 'https://example.com/',
          // Intentionally leave this URL empty so the fallback is used.
          imageUrl: '',
        },
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const faviconMainImage = $$<HTMLElement>(tableElement, '.favicon');
    assertTrue(!!faviconMainImage);
    assertEquals(
        getFaviconForPageURL('https://example.com/', false, '', 32),
        faviconMainImage.style.backgroundImage);

    // Ensure the alternate image tag isn't showing.
    const crAutoImg = $$<CrAutoImgElement>(tableElement, '.col img');
    assertFalse(!!crAutoImg);
  });

  test('product rows show the correct data', async () => {
    // Arrange.
    const productDetails1 = [
      {
        title: 'foo',
        content: {
          attributes: [{label: '', value: 'fooDescription1'}],
          summary: [{
            text: 'fooSummary',
            urls: [
              {
                url: {url: 'http://example.com/citation1'},
                title: '',
                faviconUrl: {url: ''},
                thumbnailUrl: {url: ''},
              },
              {
                url: {url: 'http://example.com/citation2'},
                title: '',
                faviconUrl: {url: ''},
                thumbnailUrl: {url: ''},
              },
            ],
          }],
        },
      },
      {
        title: 'bar',
        content: {
          attributes: [{label: '', value: 'N/A'}],
          summary: [{
            text: 'barSummary',
            urls: [],
          }],
        },
      },
    ];
    const productDetails2 = [
      {
        title: 'foo',
        content: {
          attributes: [{label: 'label', value: 'fooDescription2'}],
          summary: [{
            text: 'fooSummary2',
            urls: [
              {
                url: {url: 'http://example.com/citation1'},
                title: '',
                faviconUrl: {url: ''},
                thumbnailUrl: {url: ''},
              },
            ],
          }],
        },
      },
      {
        title: 'bar',
        content: {
          attributes: [{label: '', value: 'barDescription2'}],
          summary: [{
            text: 'barSummary2',
            urls: [],
          }],
        },
      },
    ];
    // Act.
    tableElement.columns = [
      {
        productDetails: productDetails1,
        selectedItem: {title: '', url: 'https://foo.com', imageUrl: ''},
      },
      {
        productDetails: productDetails2,
        selectedItem: {title: '', url: 'https://bar.com', imageUrl: ''},
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const titles =
        tableElement.shadowRoot!.querySelectorAll('.detail-title span');
    assertEquals(4, titles.length);
    // Titles should only show in the first column.
    assertNotStyle(titles[0]!, 'visibility', 'hidden');
    assertNotStyle(titles[1]!, 'visibility', 'hidden');
    assertStyle(titles[2]!, 'visibility', 'hidden');
    assertStyle(titles[3]!, 'visibility', 'hidden');
    assertTrue(!!titles[0]!.textContent);
    assertEquals(productDetails1[0]!.title, titles[0]!.textContent.trim());
    assertTrue(!!titles[1]!.textContent);
    assertEquals(productDetails1[1]!.title, titles[1]!.textContent.trim());

    const descriptions =
        tableElement.shadowRoot!.querySelectorAll('description-section');
    assertEquals(4, descriptions.length);
    assertDeepEquals(productDetails1[0]!.content, descriptions[0]!.description);
    assertDeepEquals(productDetails1[1]!.content, descriptions[1]!.description);
    assertDeepEquals(productDetails2[0]!.content, descriptions[2]!.description);
    assertDeepEquals(productDetails2[1]!.content, descriptions[3]!.description);
  });

  test('product rows show "text" section', async () => {
    // Arrange.
    const productDetails = [
      {
        title: 'price',
        content: '$100',
      },
    ];
    // Act.
    tableElement.columns = [
      {
        productDetails: productDetails,
        selectedItem: {title: '', url: 'https://foo.com', imageUrl: ''},
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const text = $$(tableElement, '.detail-text');
    assertTrue(!!text);
    // Titles should only show in the first column.
    assertNotStyle(text, 'visibility', 'hidden');
    assertTrue(!!text!.textContent);
    assertEquals(productDetails[0]!.content, text!.textContent.trim());
  });

  test('fires url change event', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {title: 'title', url: 'https://foo.com', imageUrl: ''},
        productDetails: [],
      },
      {
        selectedItem: {title: 'title2', url: 'https://bar.com', imageUrl: ''},
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Act
    const productSelector = $$(tableElement, 'product-selector');
    assertTrue(!!productSelector);
    const eventPromise = eventToPromise('url-change', tableElement);
    productSelector!.dispatchEvent(new CustomEvent('selected-url-change', {
      detail: {
        url: 'https://foo.com',
      },
    }));

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals('https://foo.com', event.detail.url);
    assertEquals(0, event.detail.index);
  });

  test('fires url remove event', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {title: 'title', url: 'https://foo.com', imageUrl: ''},
        productDetails: [],
      },
      {
        selectedItem: {title: 'title2', url: 'https://bar.com', imageUrl: ''},
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);
    const productSelector = $$(tableElement, 'product-selector');
    assertTrue(!!productSelector);
    const eventPromise = eventToPromise('url-remove', tableElement);
    productSelector!.dispatchEvent(new CustomEvent('remove-url'));

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals(0, event.detail.index);
  });

  test('opens tab when `openTabButton` is clicked', async () => {
    // Arrange
    const testUrl = 'https://example.com';
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: testUrl,
          imageUrl: 'https://example.com/image',
        },
        productDetails: [],
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
        productDetails: [],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Act
    const openTabButton = $$<HTMLElement>(tableElement, '.open-tab-button');
    assertTrue(!!openTabButton);
    openTabButton!.click();

    // Assert.
    assertEquals(1, shoppingServiceApi.getCallCount('switchToOrOpenTab'));
    assertEquals(
        testUrl, shoppingServiceApi.getArgs('switchToOrOpenTab')[0].url);
  });

  test('shows open tab button when hovered', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
        productDetails: [
          {
            title: 'foo',
            content: {
              attributes: [{label: '', value: 'fooDescription'}],
              summary: [{
                text: 'fooSummary',
                urls: [],
              }],
            },
          },
          {
            title: 'bar',
            content: {
              attributes: [{label: '', value: 'barDescription'}],
              summary: [{
                text: 'barSummary',
                urls: [],
              }],
            },
          },
        ],
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
        productDetails: [
          {
            title: 'foo',
            content: {
              attributes: [{label: '', value: 'fooDescription1'}],
              summary: [{
                text: 'fooSummary1',
                urls: [],
              }],
            },
          },
        ],
      },
    ];
    await flushTasks();
    const columns = tableElement.shadowRoot!.querySelectorAll('.col');
    assertEquals(2, columns.length);
    const openTabButton1 =
        columns[0]!.querySelector<HTMLElement>('.open-tab-button');
    const openTabButton2 =
        columns[1]!.querySelector<HTMLElement>('.open-tab-button');
    assertTrue(!!openTabButton1);
    assertTrue(!!openTabButton2);
    tableElement.$.table.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(isVisible(openTabButton1));
    assertFalse(isVisible(openTabButton2));

    // Act/Assert
    columns[0]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertTrue(isVisible(openTabButton1));
    assertFalse(isVisible(openTabButton2));

    columns[1]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertFalse(isVisible(openTabButton1));
    assertTrue(isVisible(openTabButton2));

    tableElement.$.table.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(isVisible(openTabButton1));
    assertFalse(isVisible(openTabButton2));
  });

  test(
      'clicking `openTabButton` while offline fires ' +
          '`unavailable-action-attempted` event',
      async () => {
        // Arrange
        tableElement.columns = [
          {
            selectedItem: {
              title: 'title',
              url: 'https://example.com',
              imageUrl: 'https://example.com/image',
            },
            productDetails: [],
          },
          {
            selectedItem: {
              title: 'title2',
              url: 'https://example.com/2',
              imageUrl: 'https://example.com/2/image',
            },
            productDetails: [],
          },
        ];
        await waitAfterNextRender(tableElement);

        // Act
        windowProxy.setResultFor('onLine', false);
        const openTabButton = $$<HTMLElement>(tableElement, '.open-tab-button');
        assertTrue(!!openTabButton);
        const eventPromise =
            eventToPromise('unavailable-action-attempted', tableElement);
        openTabButton.click();

        // Assert
        const event = await eventPromise;
        assertTrue(!!event);
        assertEquals(0, shoppingServiceApi.getCallCount('switchToOrOpenTab'));
      });

  test('descriptions hidden if empty or N/A', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [],
            summary: [{
              text: 'foo1',
              urls: [],
            }],
          },
        }],
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [{label: '', value: 'N/A'}],
            summary: [{
              text: 'foo2',
              urls: [],
            }],
          },
        }],
      },
    ];
    await waitAfterNextRender(tableElement);
    const descriptions =
        tableElement.shadowRoot!.querySelectorAll('description-text');
    assertEquals(0, descriptions.length);
  });

  test('summaries hidden if empty or N/A', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [{label: '', value: 'foo1'}],
            summary: [{
              text: 'N/A',
              urls: [],
            }],
          },
        }],
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [{label: '', value: 'foo2'}],
            summary: [],
          },
        }],
      },
    ];
    await waitAfterNextRender(tableElement);
    const descriptions =
        tableElement.shadowRoot!.querySelectorAll('description-section');
    descriptions.forEach(description => {
      assertEquals(0, description.description.summary.length);
    });
  });

  test('details hidden if no valid summaries or descriptions', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [],
            summary: [{
              text: 'N/A',
              urls: [],
            }],
          },
        }],
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
        productDetails: [{
          title: 'foo',
          content: {
            attributes: [{label: '', value: 'N/A'}],
            summary: [{
              text: 'N/A',
              urls: [],
            }],
          },
        }],
      },
    ];
    await waitAfterNextRender(tableElement);
    const details =
        tableElement.shadowRoot!.querySelectorAll('.detail-container');
    assertEquals(2, details.length);
    assertFalse(isVisible((details[0]!)));
    assertFalse(isVisible((details[1]!)));
  });

  suite('Buying options', () => {
    setup(async () => {
      tableElement.columns = [
        {
          selectedItem: {
            title: 'title',
            url: 'https://example.com',
            imageUrl: 'https://example.com/image',
          },
          productDetails: [
            {
              title: 'foo',
              content: 'summary1',
            },
            {title: null, content: {jackpotUrl: 'https://example.com/jackpot'}},
          ],
        },
        {
          selectedItem: {
            title: 'title2',
            url: 'https://example.com/2',
            imageUrl: 'https://example.com/2/image',
          },
          productDetails: [
            {title: 'foo', content: 'summary2'},
            {title: null, content: {jackpotUrl: ''}},
          ],
        },
      ];
      await waitAfterNextRender(tableElement);
    });

    test('detail title is hidden if it is `null`', async () => {
      const titles = tableElement.shadowRoot!.querySelectorAll('.detail-title');
      assertEquals(2, titles.length);
    });

    test('buying options are visible if any are available', async () => {
      const buyingOptions =
          tableElement.shadowRoot!.querySelectorAll('buying-options-section');
      assertEquals(1, buyingOptions.length);
      assertEquals('https://example.com/jackpot', buyingOptions[0]!.jackpotUrl);
    });

    test('buying options are hidden if none are available', async () => {
      assertTrue(!!tableElement.columns[0]);
      tableElement.columns = [
        {
          selectedItem: {
            title: 'title',
            url: 'https://example.com',
            imageUrl: 'https://example.com/image',
          },
          productDetails: [
            {title: null, content: {jackpotUrl: ''}},
          ],
        },
        {
          selectedItem: {
            title: 'title2',
            url: 'https://example.com/2',
            imageUrl: 'https://example.com/2/image',
          },
          productDetails: [
            {title: null, content: {jackpotUrl: ''}},
          ],
        },
      ];
      await flushTasks();

      const buyingOptions =
          tableElement.shadowRoot!.querySelectorAll('buying-options-section');
      assertEquals(0, buyingOptions.length);
    });

    test('`grid-row` populates correctly', async () => {
      // Arrange
      tableElement.columns = [
        {
          selectedItem: {
            title: 'title',
            url: 'https://example.com',
            imageUrl: 'https://example.com/image',
          },
          productDetails: [{
            title: 'foo',
            content: {attributes: [{label: '', value: 'foo1'}], summary: []},
          }],
        },
        {
          selectedItem: {
            title: 'title2',
            url: 'https://example.com/2',
            imageUrl: 'https://example.com/2/image',
          },
          productDetails: [{
            title: 'foo',
            content: {attributes: [{label: '', value: 'foo2'}], summary: []},
          }],
        },
      ];
      await waitAfterNextRender(tableElement);
      const columns = tableElement.shadowRoot!.querySelectorAll('.col');
      assertEquals(2, columns.length);
      assertStyle(columns[0]!, 'grid-row', 'span 3');
      assertStyle(columns[1]!, 'grid-row', 'span 3');
    });
  });

  suite('DragAndDrop', () => {
    test('hides open tab button when dragging', async () => {
      // Arrange
      tableElement.columns = [
        {
          selectedItem: {
            title: 'title',
            url: 'https://example.com',
            imageUrl: 'https://example.com/image',
          },
          productDetails: [
            {
              title: 'foo',
              content: {
                attributes: [{label: '', value: 'd1'}],
                summary: [],
              },
            },
          ],
        },
      ];
      await flushTasks();
      const column = $$<HTMLElement>(tableElement, '.col');
      assertTrue(!!column);
      const openTabButton =
          column!.querySelector<HTMLElement>('.open-tab-button');
      assertTrue(!!openTabButton);
      tableElement.$.table.dispatchEvent(new PointerEvent('pointerleave'));
      assertFalse(isVisible(openTabButton));

      // Act/Assert
      column!.dispatchEvent(new PointerEvent('pointerenter'));
      assertTrue(isVisible(openTabButton));

      tableElement.draggingColumn = column!;
      assertFalse(isVisible(openTabButton));
    });

    test('sets `is-first-column` attribute correctly', async () => {
      // Arrange / Act.
      tableElement.columns = [
        {
          selectedItem:
              {title: 'title', url: 'https://example.com/1', imageUrl: ''},
          productDetails: [
            {
              title: 'foo',
              content: {
                attributes: [{label: '', value: 'd1'}],
                summary: [],
              },
            },
          ],
        },
        {
          selectedItem:
              {title: 'title2', url: 'https://example.com/2', imageUrl: ''},
          productDetails: [
            {title: 'foo', content: null},
          ],
        },
      ];
      await waitAfterNextRender(tableElement);

      // Assert.
      const columns =
          tableElement.shadowRoot!.querySelectorAll<HTMLElement>('.col');
      assertEquals(2, columns.length);
      assertTrue(columns[0]!.hasAttribute('is-first-column'));
      assertFalse(columns[1]!.hasAttribute('is-first-column'));

      tableElement.draggingColumn = columns[0]!;
      // Attribute toggling should be handled by drag and drop manager.
      assertFalse(columns[0]!.hasAttribute('is-first-column'));
      assertFalse(columns[1]!.hasAttribute('is-first-column'));
    });
  });

  test('sets `scroll-snap-align` property correctly', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem:
            {title: 'title', url: 'https://example.com/1', imageUrl: ''},
        productDetails: [
          {
            title: 'foo',
            content: {
              attributes: [{label: '', value: 'd1'}],
              summary: [],
            },
          },
        ],
      },
      {
        selectedItem:
            {title: 'title2', url: 'https://example.com/2', imageUrl: ''},
        productDetails: [
          {title: 'foo', content: null},
        ],
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const columns =
        tableElement.shadowRoot!.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    assertStyle(columns[0]!, 'scroll-snap-align', 'start');
    assertStyle(columns[1]!, 'scroll-snap-align', 'start');

    tableElement.draggingColumn = columns[0]!;
    assertStyle(columns[0]!, 'scroll-snap-align', 'none');
    assertStyle(columns[1]!, 'scroll-snap-align', 'none');
  });
});

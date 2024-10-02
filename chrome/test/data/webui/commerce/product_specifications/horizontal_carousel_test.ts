// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/horizontal_carousel.js';
import 'chrome://compare/table.js';

import type {TableColumn} from 'chrome://compare/app.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, isVisible, whenCheck} from 'chrome://webui-test/test_util.js';

suite('HorizontalCarouselTest', () => {
  async function setupColumns({numColumns}: {numColumns: number}) {
    const tableElement =
        document.body.querySelector('product-specifications-table');
    if (!tableElement) {
      return;
    }
    const columns: TableColumn[] = [];
    for (let i = 0; i < numColumns; i++) {
      columns.push({
        selectedItem: {
          title: `${i}`,
          url: `https://${i}`,
          imageUrl: `https://${i}`,
        },
        productDetails: [],
      });
    }
    const carouselElement = document.body.querySelector('horizontal-carousel')!;
    carouselElement.style.maxWidth = '1140px';
    const eventPromise =
        eventToPromise('intersection-observed', carouselElement);
    tableElement.columns = columns;
    const event = await eventPromise;
    assertTrue(!!event);
  }

  function isVisibleWithinContainer(
      child: HTMLElement, container: HTMLElement): boolean {
    const childRect = child.getBoundingClientRect();
    const containerRect = container.getBoundingClientRect();

    return (
        childRect.right >= containerRect.left &&
        childRect.left <= containerRect.right &&
        childRect.bottom >= containerRect.top &&
        childRect.top <= containerRect.bottom);
  }

  function parseStylePropertyPx(element: Element, property: string): number {
    return parseFloat(
        window.getComputedStyle(element).getPropertyValue(property).replace(
            'px', ''));
  }

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
            <horizontal-carousel>
              <product-specifications-table slot="table">
              </product-specifications-table>
            </horizontal-carousel>`;
  });

  [1, 4, 5, 10].forEach(numColumns => {
    test(
        `buttons render correctly for ${numColumns} columns`, async () => {
          // Arrange.
          const carouselElement =
              document.body.querySelector('horizontal-carousel')!;

          // Act.
          await setupColumns({numColumns: numColumns});

          // Assert.
          assertFalse(isVisible(carouselElement.$.backButton));
          if (numColumns < 5) {
            assertFalse(isVisible(carouselElement.$.forwardButton));
          } else {
            assertTrue(isVisible(carouselElement.$.forwardButton));
          }
        });
  });

  test('clicking forward button surfaces back button', async () => {
    // Arrange.
    await setupColumns({numColumns: 6});
    const carouselElement = document.body.querySelector('horizontal-carousel')!;
    assertTrue(isVisible(carouselElement.$.forwardButton));
    assertFalse(isVisible(carouselElement.$.backButton));

    // Act.
    const eventPromise =
        eventToPromise('intersection-observed', carouselElement);
    carouselElement.$.forwardButton.click();
    const event = await eventPromise;

    // Assert.
    assertTrue(!!event);
    assertTrue(isVisible(carouselElement.$.backButton));
  });

  test('clicking back button resurfaces forward button', async () => {
    // Arrange.
    await setupColumns({numColumns: 6});
    const carouselElement = document.body.querySelector('horizontal-carousel')!;
    assertTrue(isVisible(carouselElement.$.forwardButton));

    // Scroll to the end of the carousel, to ensure the back button has
    // something to scroll back.
    const carouselContainer = carouselElement.$.carouselContainer;
    carouselContainer.scrollTo({left: carouselContainer.scrollWidth});
    // Wait until the forward button is hidden, not when 'intersection-observed'
    // fires, as manual scrolling may trigger it more than once.
    await whenCheck(
        carouselElement.$.forwardButton,
        () => !isVisible(carouselElement.$.forwardButton));
    assertTrue(isVisible(carouselElement.$.backButton));

    // Act.
    const eventPromise =
        eventToPromise('intersection-observed', carouselElement);
    carouselElement.$.backButton.click();
    const event = await eventPromise;

    // Assert.
    assertTrue(!!event);
    assertTrue(isVisible(carouselElement.$.forwardButton));
  });

  test('focusing on carousel item scrolls item into view', async () => {
    // Arrange.
    await setupColumns({numColumns: 6});
    const carouselElement = document.body.querySelector('horizontal-carousel')!;
    assertTrue(isVisible(carouselElement.$.forwardButton));
    const tableElement =
        document.body.querySelector('product-specifications-table')!;
    // Columns themselves aren't focusable, but the nested
    // `#currentProductContainer` in their `product-selector`s are.
    const productSelectors =
        tableElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.col product-selector');
    assertEquals(6, productSelectors.length);
    let numScrollEnd = 0;
    const carouselContainer = carouselElement.$.carouselContainer;
    carouselContainer.addEventListener('scrollend', () => {
      numScrollEnd++;
    });

    // Act - focus on the last column's focusable child.
    const focusableChild6 =
        $$<HTMLElement>(productSelectors[5]!, '#currentProductContainer');
    assertTrue(!!focusableChild6);
    let scrollEnd = eventToPromise('scrollend', carouselContainer);
    focusableChild6.focus();
    await scrollEnd;

    // Assert.
    // Forward button hides, and the back button shows, because we've scrolled
    // to the end of the container.
    assertEquals(1, numScrollEnd);
    assertTrue(isVisibleWithinContainer(focusableChild6, carouselContainer));
    await whenCheck(
        carouselElement.$.forwardButton,
        () => !isVisible(carouselElement.$.forwardButton));
    assertTrue(isVisible(carouselElement.$.backButton));

    // Act - focus on the first column's focusable child.
    const focusableChild1 =
        $$<HTMLElement>(productSelectors[0]!, '#currentProductContainer');
    assertTrue(!!focusableChild1);
    scrollEnd = eventToPromise('scrollend', carouselContainer);
    focusableChild1.focus();
    await scrollEnd;

    // Assert.
    // The back button hides, and the forward button shows, since we scrolled
    // back to the start of the container.
    assertEquals(2, numScrollEnd);
    assertTrue(isVisibleWithinContainer(focusableChild1, carouselContainer));
    await whenCheck(
        carouselElement.$.backButton,
        () => !isVisible(carouselElement.$.backButton));
    assertTrue(isVisible(carouselElement.$.forwardButton));
  });

  test(
      'buttons appear in the middle of the container when the carousel height' +
          ' is smaller than the viewport height',
      async () => {
        await setupColumns({numColumns: 6});
        const carouselElement =
            document.body.querySelector('horizontal-carousel')!;
        const carouselContainer = carouselElement.$.carouselContainer;

        // Force the carousel container to be smaller than the viewport.
        document.body.style.height = '500px';
        carouselElement.style.height = '200px';

        // Back and forward buttons should lie within carousel container.
        const backButtonTopPx =
            parseStylePropertyPx(carouselElement.$.backButtonContainer, 'top');
        const forwardButtonTopPx = parseStylePropertyPx(
            carouselElement.$.forwardButtonContainer, 'top');
        const carouselContainerTop =
            carouselContainer.getBoundingClientRect().top;
        const carouselContainerBottom =
            carouselContainer.getBoundingClientRect().bottom;

        assertGT(backButtonTopPx, carouselContainerTop);
        assertLT(backButtonTopPx, carouselContainerBottom);
        assertGT(forwardButtonTopPx, carouselContainerTop);
        assertLT(forwardButtonTopPx, carouselContainerBottom);
      });

  test(
      'buttons appear in the middle of the viewport when the carousel height' +
          ' is larger than the viewport height',
      async () => {
        await setupColumns({numColumns: 6});
        const carouselElement =
            document.body.querySelector('horizontal-carousel')!;
        const carouselContainer = carouselElement.$.carouselContainer;

        // Force the carousel container to be much larger than the viewport.
        document.body.style.height = '200px';
        carouselElement.style.height = '1000px';

        // Back and forward buttons should lie within carousel container and
        // the viewport.
        const backButtonTopPx =
            parseStylePropertyPx(carouselElement.$.backButtonContainer, 'top');
        const forwardButtonTopPx = parseStylePropertyPx(
            carouselElement.$.forwardButtonContainer, 'top');
        const carouselContainerTop =
            carouselContainer.getBoundingClientRect().top;
        const viewportBottom = window.innerHeight;

        assertGT(backButtonTopPx, carouselContainerTop);
        assertLT(backButtonTopPx, viewportBottom);
        assertGT(forwardButtonTopPx, carouselContainerTop);
        assertLT(forwardButtonTopPx, viewportBottom);
      });
});

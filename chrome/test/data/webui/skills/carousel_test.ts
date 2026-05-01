// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/carousel.js';

import type {SkillsCarouselElement} from 'chrome://skills/carousel.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SkillsCarousel', function() {
  let carousel: SkillsCarouselElement;
  const CARD_WIDTH = 270;
  const CARD_GAP = 8;
  let lastScrolledIndex = -1;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    carousel = document.createElement('skills-carousel');
    // These styles determine how many cards fit (visibly) in the carousel.
    carousel.style.setProperty('--skills-card-width', `${CARD_WIDTH}px`);
    carousel.style.setProperty('--skills-card-gap', `${CARD_GAP}px`);
    document.body.appendChild(carousel);
    await microtasksFinished();
    lastScrolledIndex = -1;

    // scrollIntoView() is async in tests, override it to be sync and manually
    // check behavior.
    if (carousel.$.carouselContainer) {
      HTMLElement.prototype.scrollIntoView = function(this: HTMLElement) {
        const items = carousel.$.itemsSlot.assignedElements() as HTMLElement[];
        lastScrolledIndex = items.indexOf(this);
      };
    }
  });

  function addSlottedItems(num: number) {
    for (let i = 0; i < num; i++) {
      const item = document.createElement('div');
      item.setAttribute('slot', 'items');
      item.style.width = `${CARD_WIDTH}px`;
      item.style.flexShrink = '0';
      item.textContent = `Skill Card ${i + 1}`;
      carousel.appendChild(item);
    }
  }

  test('ItemsOverflowScrollsCorrectAmount', async function() {
    carousel.style.width = '400px';
    addSlottedItems(4);
    await microtasksFinished();

    // Clicking the next button should scroll forward by 1.
    carousel.$.nextButton.click();
    await microtasksFinished();
    assertEquals(lastScrolledIndex, 1);


    // Clicking the prev button should scroll back to the start.
    carousel.$.prevButton.click();
    await microtasksFinished();
    assertEquals(lastScrolledIndex, 0);
  });

  suite('RTL', function() {
    setup(async function() {
      document.documentElement.dir = 'rtl';
      await microtasksFinished();
    });

    teardown(function() {
      document.documentElement.dir = 'ltr';
    });

    test('ItemsOverflowScrollsCorrectAmountRTL', async function() {
      carousel.style.width = '400px';
      addSlottedItems(4);
      await microtasksFinished();

      carousel.$.nextButton.click();
      await microtasksFinished();
      assertEquals(lastScrolledIndex, 1);

      carousel.$.prevButton.click();
      await microtasksFinished();
      assertEquals(lastScrolledIndex, 0);
    });
  });
});

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SearchManager} from 'chrome://settings/settings.js';
import {getSearchManager, getTrustedHTML as getTrustedStaticHtml, showBubble} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

// clang-format on

suite('SearchSettingsTest', function() {
  let searchManager: SearchManager;

  setup(function() {
    searchManager = getSearchManager();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Test that the DOM of a node is modified as expected when a search hit
   * occurs within that node.
   */
  test('normal highlighting', function() {
    const optionText = 'FooSettingsFoo';

    document.body.innerHTML =
        getTrustedHtml(`<settings-section>
           <div id="mydiv">${optionText}</div>
         </settings-section>`);

    const section = document.querySelector('settings-section')!;
    const div = document.querySelector('#mydiv')!;

    return searchManager.search('settings', section)
        .then(function() {
          const highlightWrapper =
              div.querySelector('.search-highlight-wrapper');
          assertTrue(!!highlightWrapper);

          const originalContent = highlightWrapper.querySelector(
              '.search-highlight-original-content');
          assertTrue(!!originalContent);
          assertEquals(optionText, originalContent.textContent);

          const searchHits = highlightWrapper.querySelectorAll<HTMLElement>(
              '.search-highlight-hit');
          assertEquals(1, searchHits.length);
          assertEquals('Settings', searchHits[0]!.textContent);

          // Check that original DOM structure is restored when search
          // highlights are cleared.
          return searchManager.search('', section);
        })
        .then(function() {
          assertEquals(0, div.children.length);
          assertEquals(optionText, div.textContent);
        });
  });

  /**
   * Tests that a search hit within a <select> node causes the parent
   * settings-section to be shown and the <select> to be highlighted by a
   * bubble.
   */
  test('<select> highlighting', function() {
    document.body.innerHTML =
        getTrustedStaticHtml`<settings-section>
           <select>
             <option>Foo</option>
             <option>Settings</option>
             <option>Baz</option>
           </select>
         </settings-section>`;

    const section = document.querySelector('settings-section')!;
    const select = section.querySelector('select')!;

    return searchManager.search('settings', section)
        .then(function() {
          assertEquals(1, document.querySelectorAll('.search-bubble').length);

          // Check that original DOM structure is present even after search
          // highlights are cleared.
          return searchManager.search('', section);
        })
        .then(function() {
          const options = select.querySelectorAll('option');
          assertEquals(3, options.length);
          assertEquals('Foo', options[0]!.textContent);
          assertEquals('Settings', options[1]!.textContent);
          assertEquals('Baz', options[2]!.textContent);
        });
  });

  test('ignored elements are ignored', async function() {
    const text = 'hello';
    document.body.innerHTML =
        getTrustedHtml(`<settings-section>
           <cr-action-menu>${text}</cr-action-menu>
           <cr-dialog>${text}</cr-dialog>
           <cr-icon>${text}</cr-icon>
           <cr-icon-button>${text}</cr-icon-button>
           <cr-slider>${text}</cr-slider>
           <dialog>${text}</dialog>
           <iron-icon>${text}</iron-icon>
           <iron-list>${text}</iron-list>
           <paper-ripple>${text}</paper-ripple>
           <cr-ripple>${text}</cr-ripple>
           <paper-spinner-lite>${text}</paper-spinner-lite>
           <slot>${text}</slot>
           <content>${text}</content>
           <style>${text}</style>
           <template>${text}</template>
         </settings-section>`);

    const section = document.querySelector('settings-section')!;
    const request = await searchManager.search(text, section);
    assertEquals(0, request.getSearchResult().matchCount);
    assertFalse(request.getSearchResult().wasClearSearch);
  });

  test('no-search elements are ignored', async function() {
    const text = 'hello';
    document.body.innerHTML =
        getTrustedHtml(`<settings-section>
           <div>${text}</div>
           <div no-search>${text}</div>
         </settings-section>`);

    const section = document.querySelector('settings-section')!;
    const request = await searchManager.search(text, section);
    assertEquals(1, request.getSearchResult().matchCount);
    assertFalse(request.getSearchResult().wasClearSearch);
  });

  // Test that multiple requests for the same text correctly highlight their
  // corresponding part of the tree without affecting other parts of the tree.
  test('multiple simultaneous requests for the same text', function() {
    document.body.innerHTML =
        getTrustedStaticHtml`<settings-section>
           <div><span>Hello there</span></div>
         </settings-section>
         <settings-section>
           <div><span>Hello over there</span></div>
         </settings-section>
         <settings-section>
           <div><span>Nothing</span></div>
         </settings-section>`;

    const sections = Array.prototype.slice.call(
        document.querySelectorAll('settings-section'));

    return Promise.all(sections.map(s => searchManager.search('there', s)))
        .then(function(requests) {
          assertEquals(1, requests[0]!.getSearchResult().matchCount);
          assertEquals(1, requests[1]!.getSearchResult().matchCount);
          assertEquals(0, requests[2]!.getSearchResult().matchCount);
        });
  });

  test('highlight removed when text is changed', function() {
    const originalText = 'FooSettingsFoo';

    document.body.innerHTML =
        getTrustedHtml(`<settings-section>
          <div id="mydiv">${originalText}</div>
        </settings-section>`);

    const div = document.querySelector('#mydiv')!;
    return searchManager.search('settings', document.body).then(() => {
      assertEquals(1, div.childNodes.length);
      const highlightWrapper = div.firstChild as HTMLElement;
      assertTrue(
          highlightWrapper.classList.contains('search-highlight-wrapper'));
      const originalContent =
          highlightWrapper.querySelector('.search-highlight-original-content');
      assertTrue(!!originalContent);
      originalContent.childNodes[0]!.nodeValue = 'Foo';
      return new Promise<void>(resolve => {
        setTimeout(() => {
          assertEquals(1, div.childNodes.length);
          assertEquals('Foo', div.innerHTML);
          resolve();
        }, 1);
      });
    });
  });

  test('match text outside of a settings section', async function() {
    document.body.innerHTML = getTrustedStaticHtml`
        <div id="mydiv">Match</div>
        <settings-section></settings-section>`;

    const mydiv = document.querySelector('#mydiv')!;

    await searchManager.search('Match', document.body);

    const highlight = mydiv.querySelector('.search-highlight-wrapper');
    assertTrue(!!highlight);

    const searchHits = highlight.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('Match', searchHits[0]!.textContent);
  });

  test('bubble result count', async () => {
    document.body.innerHTML = getTrustedStaticHtml`
        <settings-section>
          <select>
            <option>nohello</option>
            <option>hello dolly!</option>
            <option>hello to you, too!</option>
            <option>you say goodbye, I say hello!</option>
          </select>
          <button></button>
        </setting-section>`;

    await searchManager.search('hello', document.body);

    const bubbles = document.querySelectorAll('.search-bubble');
    assertEquals(1, bubbles.length);
    assertEquals('4 results', bubbles[0]!.textContent);
  });

  test('showBubble() result count', () => {
    function assertResults(results: number) {
      const bubble = document.body.querySelector<HTMLElement>('.search-bubble');
      assertTrue(!!bubble);
      assertEquals(results, Number(bubble.dataset['results']));
      assertEquals(`${results} results`, bubble.textContent);
    }

    const element = document.createElement('div');
    document.body.appendChild(element);

    showBubble(element, 10, new Set(), false);
    assertResults(10);
    showBubble(element, 20, new Set(), false);
    assertResults(30);
  });

  test('diacritics', async () => {
    document.body.innerHTML = getTrustedStaticHtml`
        <settings-section>
          <select>
            <option>año de oro</option>
          </select>
          <button></button>
          danger zone
        </setting-section>`;

    await searchManager.search('an', document.body);

    const highlights = document.querySelectorAll('.search-highlight-wrapper');
    assertEquals(1, highlights.length);
    assertEquals(1, document.querySelectorAll('.search-bubble').length);
  });
});

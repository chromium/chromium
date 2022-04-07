// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';

import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';

suite('localized_link', function() {
  let localizedStringWithLink;

  function getLocalizedStringWithLinkElementHtml(localizedString, linkUrl) {
    return `<localized-link localized-string="${localizedString}"` +
        ` link-url="${linkUrl}"></localized-link>`;
  }

  test('LinkFirst', function() {
    document.body.innerHTML =
        getLocalizedStringWithLinkElementHtml(`<a>first link</a>then text`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0 id1" tabindex="0">first link</a>` +
            `<span id="id1" aria-hidden="true">then text</span>`);
  });

  test('TextLinkText', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `first text <a>then link</a> then more text`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<span id="id0" aria-hidden="true">first text </span>` +
            `<a id="id1" aria-labelledby="id0 id1 id2" tabindex="0">then link</a>` +
            `<span id="id2" aria-hidden="true"> then more text</span>`);
  });

  test('LinkLast', function() {
    document.body.innerHTML =
        getLocalizedStringWithLinkElementHtml(`first text<a>then link</a>`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<span id="id0" aria-hidden="true">first text</span>` +
            `<a id="id1" aria-labelledby="id0 id1" tabindex="0">then link</a>`);
  });

  test('PopulatedLink', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `<a>populated link</a>`, `http://google.com`);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0" tabindex="0" ` +
            `href="http://google.com" target="_blank">populated link</a>`);
  });

  test('PrepopulatedLink', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `<a href='http://google.com'>pre-populated link</a>`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a href="http://google.com" id="id0" aria-labelledby="id0" tabindex="0">` +
            `pre-populated link</a>`);
  });

  test('NoLinkPresent', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `No anchor tags in this sentence.`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `No anchor tags in this sentence.`);
  });

  test('LinkClick', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);

    return flushTasks().then(async () => {
      const localizedLink = document.body.querySelector('localized-link');
      assertTrue(!!localizedLink);
      const anchorTag = localizedLink.shadowRoot.querySelector('a');
      assertTrue(!!anchorTag);
      const localizedLinkPromise =
          eventToPromise('link-clicked', localizedLink);

      anchorTag.click();
      await Promise.all([localizedLinkPromise, flushTasks()]);
    });
  });

  test('link disabled', async function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);

    await flushTasks();
    const localizedLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const anchorTag = localizedLink.shadowRoot.querySelector('a');
    assertTrue(!!anchorTag);
    assertEquals(anchorTag.getAttribute('tabindex'), '0');
    localizedLink.linkDisabled = true;
    await flushTasks();
    assertEquals(anchorTag.getAttribute('tabindex'), '-1');
  });

  test('change localizedString', async function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);
    await flushTasks();

    const localizedLink = document.body.querySelector('localized-link');
    localizedLink.linkDisabled = true;
    const localizedLinkPromise = eventToPromise('link-clicked', localizedLink);
    await flushTasks();

    localizedLink.localizedString = `Different text with <a href='#'>link</a>`;
    await flushTasks();

    // Tab index is still -1 due to it being disabled.
    const anchorTag = localizedLink.shadowRoot.querySelector('a');
    assertTrue(!!anchorTag);
    assertEquals(anchorTag.getAttribute('tabindex'), '-1');

    localizedLink.linkDisabled = false;
    await flushTasks();

    // Clicking the link still fires the link-clicked event.
    anchorTag.click();
    await Promise.all([localizedLinkPromise, flushTasks()]);
  });
});

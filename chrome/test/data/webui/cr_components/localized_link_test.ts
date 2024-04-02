// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';

import type {LocalizedLinkElement} from '//resources/cr_components/localized_link/localized_link.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

suite('localized_link', function() {
  let localizedStringWithLink: LocalizedLinkElement|null;

  function getLocalizedStringWithLinkElementHtml(
      localizedString: string, linkUrl: string): TrustedHTML {
    return getTrustedHtml(
        `<localized-link localized-string="${localizedString}"` +
        ` link-url="${linkUrl}"></localized-link>`);
  }

  test('LinkFirst', function() {
    document.body.innerHTML =
        getLocalizedStringWithLinkElementHtml(`<a>first link</a>then text`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedStringWithLink);
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0 id1" tabindex="0">first link</a>` +
            `<span id="id1" aria-hidden="true">then text</span>`);
  });

  test('TextLinkText', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `first text <a>then link</a> then more text`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedStringWithLink);
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
    assertTrue(!!localizedStringWithLink);
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<span id="id0" aria-hidden="true">first text</span>` +
            `<a id="id1" aria-labelledby="id0 id1" tabindex="0">then link</a>`);
  });

  test('PopulatedLink', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `<a>populated link</a>`, `https://google.com`);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedStringWithLink);
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0" tabindex="0" ` +
            `href="https://google.com" target="_blank">populated link</a>`);
  });

  test('PrepopulatedLink', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `<a href='https://google.com'>pre-populated link</a>`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedStringWithLink);
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a href="https://google.com" id="id0" aria-labelledby="id0" tabindex="0">` +
            `pre-populated link</a>`);
  });

  test('NoLinkPresent', function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `No anchor tags in this sentence.`, ``);
    localizedStringWithLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedStringWithLink);
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `No anchor tags in this sentence.`);
  });

  test('LinkClick', async () => {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);
    const localizedLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const anchorTag = localizedLink.shadowRoot!.querySelector('a');
    assertTrue(!!anchorTag);
    const localizedLinkPromise = eventToPromise('link-clicked', localizedLink);

    anchorTag.click();
    await localizedLinkPromise;
  });

  test('LinkAuxclick', async () => {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);
    const localizedLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const anchorTag = localizedLink.shadowRoot!.querySelector('a');
    assertTrue(!!anchorTag);
    const localizedLinkPromise = eventToPromise('link-clicked', localizedLink);

    // simulate a middle-button click
    anchorTag.dispatchEvent(new MouseEvent('auxclick', {button: 1}));

    await localizedLinkPromise;
  });

  test('link disabled', async function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);
    const localizedLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const anchorTag = localizedLink.shadowRoot!.querySelector('a');
    assertTrue(!!anchorTag);
    assertEquals(anchorTag.getAttribute('tabindex'), '0');
    localizedLink.linkDisabled = true;
    await microtasksFinished();
    assertEquals(anchorTag.getAttribute('tabindex'), '-1');
  });

  test('change localizedString', async function() {
    document.body.innerHTML = getLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);
    const localizedLink = document.body.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const localizedLinkPromise = eventToPromise('link-clicked', localizedLink);
    localizedLink.linkDisabled = true;
    localizedLink.localizedString = `Different text with <a href='#'>link</a>`;
    await microtasksFinished();

    // Tab index is still -1 due to it being disabled.
    const anchorTag = localizedLink.shadowRoot!.querySelector('a');
    assertTrue(!!anchorTag);
    assertEquals(anchorTag.getAttribute('tabindex'), '-1');

    localizedLink.linkDisabled = false;
    await microtasksFinished();

    // Clicking the link still fires the link-clicked event.
    anchorTag.click();
    await localizedLinkPromise;
  });
});

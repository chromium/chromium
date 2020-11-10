// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

suite('localized_link', function() {
  let localizedStringWithLink;

  function GetLocalizedStringWithLinkElementHtml(localizedString, linkUrl) {
    return `<settings-localized-link localized-string="${localizedString}"` +
        ` link-url="${linkUrl}"></settings-localized-link>`;
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('LinkFirst', function() {
    document.body.innerHTML =
        GetLocalizedStringWithLinkElementHtml(`<a>first link</a>then text`, ``);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0 id1">first link</a>` +
            `<span id="id1" aria-hidden="true">then text</span>`);
  });

  test('TextLinkText', function() {
    document.body.innerHTML = GetLocalizedStringWithLinkElementHtml(
        `first text <a>then link</a> then more text`, ``);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<span id="id0" aria-hidden="true">first text </span>` +
            `<a id="id1" aria-labelledby="id0 id1 id2">then link</a>` +
            `<span id="id2" aria-hidden="true"> then more text</span>`);
  });

  test('LinkLast', function() {
    document.body.innerHTML =
        GetLocalizedStringWithLinkElementHtml(`first text<a>then link</a>`, ``);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<span id="id0" aria-hidden="true">first text</span>` +
            `<a id="id1" aria-labelledby="id0 id1">then link</a>`);
  });

  test('PopulatedLink', function() {
    document.body.innerHTML = GetLocalizedStringWithLinkElementHtml(
        `<a>populated link</a>`, `http://google.com`);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a id="id0" aria-labelledby="id0" href="http://google.com" ` +
            `target="_blank">populated link</a>`);
  });

  test('PrepopulatedLink', function() {
    document.body.innerHTML = GetLocalizedStringWithLinkElementHtml(
        `<a href='http://google.com'>pre-populated link</a>`, ``);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `<a href="http://google.com" id="id0" aria-labelledby="id0">` +
            `pre-populated link</a>`);
  });

  test('NoLinkPresent', function() {
    document.body.innerHTML = GetLocalizedStringWithLinkElementHtml(
        `No anchor tags in this sentence.`, ``);
    localizedStringWithLink =
        document.body.querySelector('settings-localized-link');
    assertEquals(
        localizedStringWithLink.$.container.innerHTML,
        `No anchor tags in this sentence.`);
  });

  test('LinkClick', function() {
    document.body.innerHTML = GetLocalizedStringWithLinkElementHtml(
        `Text with a <a href='#'>link</a>`, ``);

    return flushAsync().then(async () => {
      const localizedLink =
        document.body.querySelector('settings-localized-link');
      assertTrue(!!localizedLink);
      const anchorTag = localizedLink.$$('a');
      assertTrue(!!anchorTag);
      const localizedLinkPromise = test_util.eventToPromise(
        'link-clicked', localizedLink);

      anchorTag.click();
      await Promise.all([localizedLinkPromise, test_util.flushTasks()]);
    });
  });
});

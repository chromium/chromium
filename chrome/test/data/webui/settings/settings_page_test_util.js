// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from '../chai_assert.js';
// clang-format on

/** @fileoverview Test utils for Settings page tests. */

/**
 * @param {string} type The settings page type, e.g. 'about' or 'basic'.
 * @return {!Promise<!HTMLElement>} The PolymerElement for the page.
 */
export function getPage(type) {
  const settingsUi = document.querySelector('settings-ui');
  assertTrue(!!settingsUi);
  const settingsMain = settingsUi.shadowRoot.querySelector('settings-main');
  assertTrue(!!settingsMain);
  const page = settingsMain.shadowRoot.querySelector(`settings-${type}-page`);

  const idleRender =
      page && page.shadowRoot.querySelector('settings-idle-load');
  if (!idleRender) {
    return Promise.resolve(page);
  }

  return idleRender.get().then(function() {
    flush();
    return page;
  });
}

/**
 * @param {!HTMLElement} page The PolymerElement for the page containing
 *     |section|.
 * @param {string} section The settings page section, e.g. 'appearance'.
 * @return {Node|undefined} The DOM node for the section.
 */
export function getSection(page, section) {
  const sections = page.shadowRoot.querySelectorAll('settings-section');
  assertTrue(!!sections);
  for (let i = 0; i < sections.length; ++i) {
    const s = sections[i];
    if (s.section === section) {
      return s;
    }
  }
  return undefined;
}

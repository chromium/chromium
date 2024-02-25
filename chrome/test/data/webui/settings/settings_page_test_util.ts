// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSectionElement} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

/** @fileoverview Test utils for Settings page tests. */

/**
 * @param type The settings page type, e.g. 'about' or 'basic'.
 * @return The PolymerElement for the page.
 */
export function getPage(type: 'basic'|'about'): Promise<HTMLElement> {
  const settingsUi = document.querySelector('settings-ui');
  assertTrue(!!settingsUi);
  const settingsMain = settingsUi!.shadowRoot!.querySelector('settings-main');
  assertTrue(!!settingsMain);
  const page = settingsMain!.shadowRoot!.querySelector<HTMLElement>(
      `settings-${type}-page`);
  assertTrue(!!page);

  const idleRender =
      page && page.shadowRoot!.querySelector('settings-idle-load');
  if (!idleRender) {
    return Promise.resolve(page);
  }

  return idleRender.get().then(function() {
    flush();
    return page;
  });
}

/**
 * @param page The PolymerElement for the page containing |section|.
 * @param section The settings page section, e.g. 'appearance'.
 * @return The DOM node for the section.
 */
export function getSection(
    page: HTMLElement, section: string): SettingsSectionElement|undefined {
  const sections = page.shadowRoot!.querySelectorAll('settings-section');
  assertTrue(!!sections);
  for (const s of sections) {
    if (s.section === section) {
      return s;
    }
  }
  return undefined;
}

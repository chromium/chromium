// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSetsSidebarElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SideBarTest', () => {
  let sidebar: RelatedWebsiteSetsSidebarElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('related-website-sets-sidebar');
    document.body.appendChild(sidebar);
  });

  test('check layout', () => {
    assertTrue(isVisible(sidebar));
    const renderedLinks = sidebar.shadowRoot!.querySelectorAll('a');
    assertEquals(1, renderedLinks.length);
    assertEquals('chrome://settings/', renderedLinks[0]!.href);
    assertEquals('Settings', renderedLinks[0]!.textContent!.trim());
  });
});

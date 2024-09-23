// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSetsToolbarElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('ToolbarTest', () => {
  let toolbar: RelatedWebsiteSetsToolbarElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('related-website-sets-toolbar');
    document.body.appendChild(toolbar);
  });

  // TODO(crgbug.com/348573599): Add search bar input functionality test
  test('check layout', () => {
    assertTrue(isVisible(toolbar));
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContextualEntrypointAndCarouselElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageContextualEntrypointAndCarouselTest', () => {
  let element: ContextualEntrypointAndCarouselElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new ContextualEntrypointAndCarouselElement();
    document.body.appendChild(element);
  });

  test('voice search click emits event', async () => {
    element.searchboxLayoutMode = 'TallTopContext';
    element.showDropdown = true;
    element.showVoiceSearch = true;
    await microtasksFinished();

    const whenOpenVoiceSearch = eventToPromise('open-voice-search', element);

    const voiceSearchButton = element.$.voiceSearchButton;
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();

    await whenOpenVoiceSearch;
  });
});

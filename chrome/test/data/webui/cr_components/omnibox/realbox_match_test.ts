// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/omnibox/realbox_match.js';

import {NavigationPredictor} from 'chrome://resources/cr_components/omnibox/omnibox.mojom-webui.js';
import {RealboxBrowserProxy} from 'chrome://resources/cr_components/omnibox/realbox_browser_proxy.js';
import {RealboxMatchElement} from 'chrome://resources/cr_components/omnibox/realbox_match.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createAutocompleteMatch, TestRealboxBrowserProxy} from './realbox_test_utils.js';

suite('CrComponentsRealboxMatchTest', () => {
  let matchEl: RealboxMatchElement;
  let testProxy: TestRealboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestRealboxBrowserProxy();
    RealboxBrowserProxy.setInstance(testProxy);

    matchEl = document.createElement('cr-realbox-match');
    matchEl.match = createAutocompleteMatch();
    matchEl.matchIndex = 0;
    document.body.appendChild(matchEl);

    await flushTasks();
  });

  test('MousedownEventsAreSentToHandler', async () => {
    const matchIndex = 2;
    const destinationUrl = {url: 'http://google.com'};
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    matchEl.dispatchEvent(new MouseEvent('mousedown'));
    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(matchIndex, args[0]);
    assertEquals(destinationUrl, args[1]);
    assertEquals(NavigationPredictor.kMouseDown, args[2]);
  });
});

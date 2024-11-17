// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://flags/app.js';

import type {FlagsAppElement} from 'chrome://flags/app.js';
import {FlagsBrowserProxyImpl} from 'chrome://flags/flags_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestFlagsBrowserProxy} from './test_flags_browser_proxy.js';

suite('chrome://flags/deprecated', function() {
  let app: FlagsAppElement;
  let searchTextArea: HTMLInputElement;
  let browserProxy: TestFlagsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestFlagsBrowserProxy();
    FlagsBrowserProxyImpl.setInstance(browserProxy);
    app = document.createElement('flags-app');
    document.body.appendChild(app);
    app.setAnnounceStatusDelayMsForTesting(0);
    app.setSearchDebounceDelayMsForTesting(0);
    searchTextArea = app.getRequiredElement<HTMLInputElement>('#search');
  });

  test('RequestDeprecatedFeatures', function() {
    return browserProxy.whenCalled('requestDeprecatedFeatures');
  });

  test('Strings', function() {
    assertEquals(loadTimeData.getString('deprecatedTitle'), document.title);
    assertEquals(
        loadTimeData.getString('deprecatedSearchPlaceholder'),
        searchTextArea.placeholder);
    assertEquals(
        loadTimeData.getString('deprecatedHeading'),
        app.getRequiredElement('.section-header-title').textContent);
    assertEquals('', app.getRequiredElement('.blurb-warning').textContent);
    assertEquals(
        loadTimeData.getString('deprecatedPageWarningExplanation'),
        app.getRequiredElement('.blurb-warning + span').textContent);
  });
});

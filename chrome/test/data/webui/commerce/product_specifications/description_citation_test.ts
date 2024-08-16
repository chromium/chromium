// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/description_citation.js';

import type {DescriptionCitationElement} from 'chrome://compare/description_citation.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('DescriptionCitationTest', () => {
  let citationElement: DescriptionCitationElement;
  const mockOpenWindowProxy = TestMock.fromClass(OpenWindowProxyImpl);

  setup(async () => {
    OpenWindowProxyImpl.setInstance(mockOpenWindowProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    citationElement = document.createElement('description-citation');
    citationElement.url = 'http://example.com/initial_url';
    document.body.appendChild(citationElement);
    await flushTasks();
  });

  test('citation renders correctly', async () => {
    citationElement.index = 4;
    citationElement.url = 'http://example.com/long/path/url';
    await flushTasks();

    assertTrue(!!citationElement.$.citation);
    assertEquals('4', citationElement.$.citation.textContent!?.trim());
    assertEquals('example.com', citationElement.$.tooltip.textContent!?.trim());
  });

  test('tooltip excludes www', async () => {
    citationElement.index = 1;
    citationElement.url = 'http://www.example.com/';
    await flushTasks();

    assertTrue(!!citationElement.$.citation);
    assertEquals('1', citationElement.$.citation.textContent!?.trim());
    assertEquals('example.com', citationElement.$.tooltip.textContent!?.trim());
  });

  test('click opens tab', async () => {
    const url = 'http://example.com/long/path/url';
    citationElement.index = 1;
    citationElement.url = url;
    await flushTasks();

    citationElement.$.citation.click();

    const arg = await mockOpenWindowProxy.whenCalled('openUrl');
    assertEquals(url, arg);
  });

  test('tooltip has no animation delay', async () => {
    assertEquals(0, citationElement.$.tooltip.animationDelay);
  });
});

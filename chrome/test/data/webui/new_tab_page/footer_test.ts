// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {FooterElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, createTheme} from './test_support.js';

suite('Footer', () => {
  let footer: FooterElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    footer = document.createElement('ntp-footer');
    document.body.appendChild(footer);
    return microtasksFinished();
  });

  suite('Background Attribution', () => {
    test('setting theme updates attribution', async () => {
      // Arrange/Act.
      footer.theme = createTheme();
      await microtasksFinished();

      // Assert.
      assertStyle(
          footer.$.footerContainer,
          '--color-new-tab-page-attribution-foreground',
          'rgba(0, 0, 255, 1.00)');
      assertStyle(footer.$.backgroundImageAttribution, 'display', 'none');
      assertStyle(footer.$.backgroundImageAttribution2, 'display', 'none');
    });

    test('setting attributions shows attributions', async function() {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImageAttribution1 = 'foo';
      theme.backgroundImageAttribution2 = 'bar';
      theme.backgroundImageAttributionUrl = {url: 'https://info.com'};

      // Act.
      footer.theme = theme;
      await microtasksFinished();

      // Assert.
      assertNotStyle(footer.$.backgroundImageAttribution, 'display', 'none');
      assertNotStyle(footer.$.backgroundImageAttribution2, 'display', 'none');
      assertEquals(
          'https://info.com',
          footer.$.backgroundImageAttribution.getAttribute('href'));
      assertEquals(
          'foo', footer.$.backgroundImageAttribution1.textContent!.trim());
      assertEquals(
          'bar', footer.$.backgroundImageAttribution2.textContent!.trim());
    });
  });
});

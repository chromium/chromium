// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {SecurityChipIcon} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {LocationIconElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('LocationIconTest', function() {
  let locationIcon: LocationIconElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    locationIcon = document.createElement('location-icon');
    document.body.appendChild(locationIcon);
  });

  test('Render text', async function() {
    locationIcon.state = {
      icon: SecurityChipIcon.kHttp,
      securityLevel: 0,
      text: 'Not secure',
      isClickable: true,
      isTextDangerous: false,
    };
    await microtasksFinished();

    const textEl = locationIcon.shadowRoot.querySelector('#text');
    assertTrue(!!textEl);
    assertEquals('Not secure', textEl.textContent);
    assertTrue(locationIcon.hasAttribute('clickable'));
    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));

    const iconContainer =
        locationIcon.shadowRoot.querySelector<HTMLElement>('#iconContainer');
    assertTrue(!!iconContainer);
    assertTrue(
        iconContainer.style.maskImage.includes('http_chrome_refresh.svg'));
  });

  test('Dangerous text', async function() {
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-dangerous-background', 'rgb(0, 0, 255)');
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-text', 'rgb(0, 255, 0)');

    locationIcon.state = {
      icon: SecurityChipIcon.kDangerous,
      securityLevel: 3,  // DANGEROUS
      text: 'Dangerous',
      isClickable: true,
      isTextDangerous: true,
    };
    await microtasksFinished();

    assertTrue(locationIcon.hasAttribute('is-text-dangerous'));
    assertTrue(locationIcon.hasAttribute('is-dangerous'));

    const container =
        locationIcon.shadowRoot.querySelector<HTMLElement>('#container');
    assertTrue(!!container);
    assertTrue(hasStyle(container, 'background-color', 'rgb(0, 0, 255)'));
    assertTrue(hasStyle(container, 'color', 'rgb(0, 255, 0)'));
  });

  test('Dangerous level, Not secure text', async function() {
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-dangerous', 'rgb(255, 0, 0)');

    locationIcon.state = {
      icon: SecurityChipIcon.kDangerous,
      securityLevel: 3,  // DANGEROUS
      text: 'Not secure',
      isClickable: true,
      isTextDangerous: false,
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));
    assertTrue(locationIcon.hasAttribute('is-dangerous'));

    const container =
        locationIcon.shadowRoot.querySelector<HTMLElement>('#container');
    assertTrue(!!container);
    assertTrue(hasStyle(container, 'color', 'rgb(255, 0, 0)'));
  });

  test('Warning text', async function() {
    locationIcon.state = {
      icon: SecurityChipIcon.kNotSecureWarning,
      securityLevel: 4,  // WARNING
      text: 'Not secure',
      isClickable: true,
      isTextDangerous: false,
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));
    assertFalse(locationIcon.hasAttribute('is-dangerous'));
  });

  test('Unclickable state', async function() {
    locationIcon.state = {
      icon: SecurityChipIcon.kHttp,
      securityLevel: 0,
      text: '',
      isClickable: false,
      isTextDangerous: false,
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('clickable'));
  });
});

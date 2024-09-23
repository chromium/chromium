// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://healthd-internals/app.js';

import type {HealthdInternalsAppElement} from 'chrome://healthd-internals/app.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('healthdInternalsTestSuite', function() {
  let healthdInternalsApp: HealthdInternalsAppElement;
  // The expected number of navigation items in the sidebar.
  const navItemsNumber: number = 7;
  // The expected number of card components in the telemetry page.
  const cardsNumberTelemetryPage: number = 5;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  function initializePage() {
    healthdInternalsApp = document.createElement('healthd-internals-app');
    assertTrue(!!healthdInternalsApp);
    document.body.appendChild(healthdInternalsApp);
    return flushTasks();
  }

  // Check if the healthd-internals app can be loaded.
  test('AppLoaded', async () => {
    await initializePage();

    // Verify the title in the app.
    const title =
        strictQuery('#pageTitle', healthdInternalsApp.shadowRoot, HTMLElement);
    assert(title.textContent);
    assertEquals('Healthd Internals', title.textContent.trim());

    // Verify the navigation buttons in the sidebar.
    const menuSelector =
        strictQuery('#selector', healthdInternalsApp.shadowRoot, HTMLElement);
    const navItems = menuSelector.querySelectorAll('.cr-nav-menu-item');
    assertEquals(navItemsNumber, navItems.length);
  });

  // Check if the telemetry page can be loaded.
  test('TelemetryPagedLoaded', async () => {
    await initializePage();

    const telemetryPage = strictQuery(
        '#telemetryPage', healthdInternalsApp.shadowRoot, HTMLElement);
    const cardContainer = strictQuery(
        '.cr-centered-card-container', telemetryPage.shadowRoot, HTMLElement);
    const cardElements = cardContainer.querySelectorAll('*');

    assertEquals(cardsNumberTelemetryPage, cardElements.length);
    assertEquals('cpuCard', cardElements[0]!.id);
    assertEquals('memoryCard', cardElements[1]!.id);
    assertEquals('fanCard', cardElements[2]!.id);
    assertEquals('powerCard', cardElements[3]!.id);
    assertEquals('thermalCard', cardElements[4]!.id);
  });

  // Check if the sidebar can be controlled by clicking the toggle button.
  test('SidebarHideAndShow', async () => {
    await initializePage();

    const sidebar =
        strictQuery('#sidebar', healthdInternalsApp.shadowRoot, HTMLElement);
    const sidebarToggleButton = strictQuery(
        '#sidebarToggleButton', healthdInternalsApp.shadowRoot, HTMLElement);

    // Sidebar is displyed by default.
    assertEquals(sidebar.hidden, false);
    assertEquals(sidebarToggleButton.innerText, '<');

    for (let index = 0; index < 10; index++) {
      // Hide the sidebar and check.
      sidebarToggleButton.click();
      assertEquals(sidebar.hidden, true);
      assertEquals(sidebarToggleButton.innerText, '>');

      // Show the sidebar and check.
      sidebarToggleButton.click();
      assertEquals(sidebar.hidden, false);
      assertEquals(sidebarToggleButton.innerText, '<');
    }
  });
});

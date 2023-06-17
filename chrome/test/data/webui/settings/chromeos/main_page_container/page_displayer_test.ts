// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PageDisplayerElement, routesMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

const {Section} = routesMojom;

suite('<page-displayer>', () => {
  let pageDisplayer: PageDisplayerElement;

  function createElement() {
    pageDisplayer = document.createElement('page-displayer');
    pageDisplayer.section = Section.kNetwork;
    document.body.appendChild(pageDisplayer);
    flush();
  }

  teardown(() => {
    pageDisplayer.remove();
  });

  suite('When OsSettingsRevampWayfinding feature flag is disabled', () => {
    setup(() => {
      // Simulate feature disabled
      loadTimeData.overrideValues({isRevampWayfindingEnabled: false});
      document.body.classList.remove('revamp-wayfinding-enabled');

      createElement();
    });

    test('should display when active', () => {
      pageDisplayer.active = true;
      assertNotEquals('none', getComputedStyle(pageDisplayer).display);
    });

    test('should display when inactive', () => {
      pageDisplayer.active = false;
      assertNotEquals('none', getComputedStyle(pageDisplayer).display);
    });
  });

  suite('When OsSettingsRevampWayfinding feature flag is enabled', () => {
    setup(() => {
      // Simulate feature enabled
      loadTimeData.overrideValues({isRevampWayfindingEnabled: true});
      document.body.classList.add('revamp-wayfinding-enabled');

      createElement();
    });

    test('should display only when active', () => {
      pageDisplayer.active = false;
      assertEquals('none', getComputedStyle(pageDisplayer).display);

      pageDisplayer.active = true;
      assertNotEquals('none', getComputedStyle(pageDisplayer).display);
    });
  });
});

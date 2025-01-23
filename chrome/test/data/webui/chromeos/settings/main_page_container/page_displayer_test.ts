// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import type {PageDisplayerElement} from 'chrome://os-settings/os_settings.js';
import {routesMojom} from 'chrome://os-settings/os_settings.js';
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

  setup(() => {
    createElement();
  });

  teardown(() => {
    pageDisplayer.remove();
  });

  test('should display only when active', () => {
    pageDisplayer.active = false;
    assertEquals('none', getComputedStyle(pageDisplayer).display);

    pageDisplayer.active = true;
    assertNotEquals('none', getComputedStyle(pageDisplayer).display);
  });
});

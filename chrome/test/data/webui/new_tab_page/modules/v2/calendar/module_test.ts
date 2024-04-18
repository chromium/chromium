// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CalendarModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {googleCalendarDescriptor, outlookCalendarDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesCalendarModuleTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  [
    {descriptor: googleCalendarDescriptor, title: 'Google Calendar'},
    {descriptor: outlookCalendarDescriptor, title: 'Outlook Calendar'},
  ].forEach(({descriptor, title}) => {
    test(`creates ${title} module`, async () => {
      loadTimeData.overrideValues({
        modulesGoogleCalendarTitle: title,
      });
      const module = await descriptor.initialize(0) as CalendarModuleElement;
      assertTrue(!!module);
      document.body.append(module);
      await waitAfterNextRender(module);

      // Assert.
      assertTrue(isVisible(module.$.moduleHeaderElementV2));
      assertEquals(module.$.moduleHeaderElementV2.headerText, title);
    });
  });
});

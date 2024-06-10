// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CalendarElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createEvents} from './test_support.js';

suite('NewTabPageModulesCalendarTest', () => {
  let element: CalendarElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new CalendarElement();
    document.body.append(element);
  });

  test('events are listed', async () => {
    const numEvents = 2;
    element.events = createEvents(numEvents);
    await waitAfterNextRender(element);

    // Assert.
    const eventElements =
        element.shadowRoot!.querySelectorAll('ntp-calendar-event');
    assertEquals(eventElements.length, numEvents);
    eventElements.forEach((element) => {
      assertTrue(isVisible(element));
    });
  });

  test('first event is expanded', async () => {
    element.events = createEvents(3);
    await waitAfterNextRender(element);

    // Assert.
    const firstEvent = $$(element, 'ntp-calendar-event');
    assertTrue(!!firstEvent);
    assertTrue(firstEvent.hasAttribute('expanded'));
  });

  test('see more link is displayed', async () => {
    element.calendarLink = 'https://foo.com/';

    // Assert.
    assertTrue(isVisible(element.$.seeMore));
    const anchor = element.$.seeMore.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!anchor);
    assertEquals(anchor!.href, 'https://foo.com/');
    assertEquals(anchor!.innerText, 'See More');
  });
});

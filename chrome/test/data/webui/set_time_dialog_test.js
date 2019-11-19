// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://set-time/set_time_dialog.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SetTimeBrowserProxyImpl} from 'chrome://set-time/set_time_browser_proxy.js';

import {TestBrowserProxy} from './test_browser_proxy.m.js';

suite('SetTimeDialog', function() {
  let setTimeElement = null;
  let testBrowserProxy = null;

  /** @implements {SetTimeBrowserProxy} */
  class TestSetTimeBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'sendPageReady', 'setTimeInSeconds', 'setTimezone', 'dialogClose',
        'doneClicked'
      ]);
    }

    /** @override */
    sendPageReady() {
      this.methodCalled('sendPageReady');
    }

    /** @override */
    setTimeInSeconds(timeInSeconds) {
      this.methodCalled('setTimeInSeconds', timeInSeconds);
    }

    /** @override */
    setTimezone(timezone) {
      this.methodCalled('setTimezone', timezone);
    }

    /** @override */
    dialogClose() {
      this.methodCalled('dialogClose');
    }

    /** @override */
    doneClicked() {
      this.methodCalled('doneClicked');
      cr.webUIListenerCallback('validation-complete');
    }
  }

  suiteSetup(function() {
    // Must use existing timezones in the test.
    loadTimeData.overrideValues({
      currentTimezoneId: 'America/Sao_Paulo',
      timezoneList: [
        [
          'America/Los_Angeles',
          '(GMT-7:00) Pacific Daylight Time (Los Angeles)'
        ],
        ['America/Sao_Paulo', '(GMT-3:00) Brasilis Standard Time (Sao Paulo)'],
        ['Asia/Seoul', '(GMT+9:00) Korean Standard Time (Seoul)'],
      ],
    });
  });

  setup(function() {
    testBrowserProxy = new TestSetTimeBrowserProxy();
    SetTimeBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    setTimeElement = document.createElement('set-time-dialog');
    document.body.appendChild(setTimeElement);
    flush();
  });

  teardown(function() {
    setTimeElement.remove();
  });

  test('PageReady', () => {
    // Verify the page sends the ready message.
    assertEquals(1, testBrowserProxy.getCallCount('sendPageReady'));
  });

  test('DateRangeContainsNow', () => {
    const dateInput = setTimeElement.$$('#dateInput');

    // Input element attributes min and max are strings like '2019-03-01'.
    const minDate = new Date(dateInput.min);
    const maxDate = new Date(dateInput.max);
    const now = new Date();

    // Verify min <= now <= max.
    assertLE(minDate, now);
    assertLE(now, maxDate);
  });

  test('SetDate', () => {
    const dateInput = setTimeElement.$$('#dateInput');
    assertTrue(!!dateInput);

    // Simulate the user changing the date picker forward by a week.
    const today = dateInput.valueAsDate;
    const nextWeek = new Date(today.getTime() + 7 * 24 * 60 * 60 * 1000);
    dateInput.focus();
    dateInput.valueAsDate = nextWeek;
    setTimeElement.$$('#doneButton').click();

    // Verify the page sends a request to move time forward.
    return testBrowserProxy.whenCalled('setTimeInSeconds')
        .then(timeInSeconds => {
          const todaySeconds = today.getTime() / 1000;
          // The exact value isn't important (it depends on the current time).
          assertGT(timeInSeconds, todaySeconds);
        });
  });

  test('Revert invalid date on blur', () => {
    const dateInput = setTimeElement.$$('#dateInput');
    dateInput.focus();
    dateInput.value = '9999-99-99';
    dateInput.blur();
    // The exact value isn't important (it depends on the current date, and
    // the date could change in the middle of the test).
    assertNotEquals('9999-99-99', dateInput.value);
  });

  test('SystemTimezoneChanged', () => {
    const timezoneSelect = setTimeElement.$$('#timezoneSelect');
    assertTrue(!!timezoneSelect);
    expectEquals('America/Sao_Paulo', timezoneSelect.value);

    cr.webUIListenerCallback('system-timezone-changed', 'America/Los_Angeles');
    expectEquals('America/Los_Angeles', timezoneSelect.value);

    cr.webUIListenerCallback('system-timezone-changed', 'Asia/Seoul');
    expectEquals('Asia/Seoul', timezoneSelect.value);
  });

  test('SetDateAndTimezone', () => {
    const dateInput = setTimeElement.$$('#dateInput');
    assertTrue(!!dateInput);

    const timeInput = setTimeElement.$$('#timeInput');
    assertTrue(!!timeInput);

    const timezoneSelect = setTimeElement.$$('#timezoneSelect');
    assertTrue(!!timezoneSelect);
    expectEquals('America/Sao_Paulo', timezoneSelect.value);

    // Simulate the user changing the time by forwarding it 15 minutes.
    const originalTime = dateInput.valueAsDate;
    originalTime.setMilliseconds(timeInput.valueAsNumber);
    const updatedTime = new Date(originalTime.getTime() + 15 * 60 * 1000);
    dateInput.focus();
    dateInput.valueAsDate = updatedTime;
    setTimeElement.$$('#doneButton').click();

    // Simulate timezone change.
    cr.webUIListenerCallback('system-timezone-changed', 'America/Los_Angeles');
    expectEquals('America/Los_Angeles', timezoneSelect.value);

    // Make sure that time on input field was updated.
    const updatedTimeAndTimezone = dateInput.valueAsDate;
    updatedTimeAndTimezone.setMilliseconds(timeInput.valueAsNumber);
    // updatedTimeAndTimezone reflects the new timezone so it should be
    // smaller, because it is more to the west than the original
    // one, therefore even with the 15 minutes forwarded it should be smaller.
    expectGT(updatedTime.getTime(), updatedTimeAndTimezone.getTime());

    assertEquals(1, testBrowserProxy.getCallCount('setTimezone'));

    return testBrowserProxy.whenCalled('setTimeInSeconds')
        .then(timeInSeconds => {
          const todaySeconds = originalTime.getTime() / 1000;
          // The exact value isn't important (it depends on the current time).
          // timeInSeconds should be bigger, because this timestamp is seconds
          // since epoch and it does not hold any information regarding the
          // current timezone.
          assertGT(timeInSeconds, todaySeconds);
        });
  });

  suite('NullTimezone', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        currentTimezoneId: '',
        timezoneList: [],
      });
    });

    test('SetDateNullTimezone', () => {
      const dateInput = setTimeElement.$$('#dateInput');
      assertTrue(!!dateInput);

      assertEquals(null, setTimeElement.$$('#timezoneSelect'));

      // Simulates the user changing the date picker backward by two days. We
      // are changing the date to make the test simpler. Changing the time
      // would require timezone manipulation and handling corner cases over
      // midnight. valuesAsDate return the time in UTC, therefore the amount
      // of days here must be bigger than one to avoid situations where the
      // new time and old time are in the same day.
      const today = dateInput.valueAsDate;
      const twoDaysAgo = new Date(today.getTime() - 2 * 24 * 60 * 60 * 1000);
      dateInput.focus();
      dateInput.valueAsDate = twoDaysAgo;
      setTimeElement.$$('#doneButton').click();

      assertEquals(0, testBrowserProxy.getCallCount('setTimezone'));

      // Verify the page sends a request to move time backward.
      return testBrowserProxy.whenCalled('setTimeInSeconds')
          .then(newTimeSeconds => {
            const todaySeconds = today.getTime() / 1000;
            // Check that the current time is bigger than the new time, which
            // is supposed to be two days ago. The exact value isn't
            // important, checking it is difficult because it depends on the
            // current time, which is constantly updated, therefore we only
            // assert that one is bigger than the other.
            assertGT(todaySeconds, newTimeSeconds);
          });
    });
  });
});

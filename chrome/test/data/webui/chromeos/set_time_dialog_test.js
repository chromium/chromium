// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://set-time/set_time.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SetTimeBrowserProxyImpl} from 'chrome://set-time/set_time_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

suite('SetTimeDialog', function() {
  let setTimeElement = null;
  let testBrowserProxy = null;

  /** @implements {SetTimeBrowserProxy} */
  class TestSetTimeBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'sendPageReady',
        'setTimeInSeconds',
        'setTimezone',
        'dialogClose',
        'doneClicked',
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
    }
  }

  suiteSetup(function() {
    // Must use existing timezones in the test.
    loadTimeData.overrideValues({
      showTimezone: true,
      currentTimezoneId: 'America/Sao_Paulo',
      timezoneList: [
        [
          'America/Los_Angeles',
          '(GMT-7:00) Pacific Daylight Time (Los Angeles)',
        ],
        ['America/Sao_Paulo', '(GMT-3:00) Brasilis Standard Time (Sao Paulo)'],
        ['Asia/Seoul', '(GMT+9:00) Korean Standard Time (Seoul)'],
      ],
    });
  });

  setup(function() {
    testBrowserProxy = new TestSetTimeBrowserProxy();
    SetTimeBrowserProxyImpl.setInstance(testBrowserProxy);
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

  test('SetDate', async () => {
    const dateInput = setTimeElement.$$('#dateInput');
    assertTrue(!!dateInput);

    // Simulate the user changing the date picker forward by a week.
    const today = dateInput.valueAsDate;
    const nextWeek = new Date(today.getTime() + 7 * 24 * 60 * 60 * 1000);
    dateInput.focus();
    dateInput.valueAsDate = nextWeek;
    setTimeElement.$$('#doneButton').click();

    // The browser validates the change.
    await testBrowserProxy.whenCalled('doneClicked');
    cr.webUIListenerCallback('validation-complete');

    // Verify the page sends a request to move time forward.
    const timeInSeconds = await testBrowserProxy.whenCalled('setTimeInSeconds');
    const todaySeconds = today.getTime() / 1000;
    // The exact value isn't important (it depends on the current time).
    assertGT(timeInSeconds, todaySeconds);
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
    assertEquals('America/Sao_Paulo', timezoneSelect.value);

    cr.webUIListenerCallback('system-timezone-changed', 'America/Los_Angeles');
    assertEquals('America/Los_Angeles', timezoneSelect.value);

    cr.webUIListenerCallback('system-timezone-changed', 'Asia/Seoul');
    assertEquals('Asia/Seoul', timezoneSelect.value);
  });

  // Disabled for flake. https://crbug.com/1043598
  test.skip('SetDateAndTimezone', async () => {
    const dateInput = setTimeElement.$$('#dateInput');
    assertTrue(!!dateInput);

    const timeInput = setTimeElement.$$('#timeInput');
    assertTrue(!!timeInput);

    const timezoneSelect = setTimeElement.$$('#timezoneSelect');
    assertTrue(!!timezoneSelect);
    assertEquals('America/Sao_Paulo', timezoneSelect.value);

    // Simulate the user changing the time by forwarding it 15 minutes.
    const originalTime = dateInput.valueAsDate;
    originalTime.setMilliseconds(timeInput.valueAsNumber);
    const updatedTime = new Date(originalTime.getTime() + 15 * 60 * 1000);
    dateInput.focus();
    dateInput.valueAsDate = updatedTime;
    dateInput.blur();

    // Simulate the user changing the time zone.
    cr.webUIListenerCallback('system-timezone-changed', 'America/Los_Angeles');
    assertEquals('America/Los_Angeles', timezoneSelect.value);

    // Make sure that time on input field was updated.
    const updatedTimeAndTimezone = dateInput.valueAsDate;
    updatedTimeAndTimezone.setMilliseconds(timeInput.valueAsNumber);
    // updatedTimeAndTimezone reflects the new timezone so it should be
    // smaller, because it is more to the west than the original
    // one, therefore even with the 15 minutes forwarded it should be smaller.
    assertGT(updatedTime.getTime(), updatedTimeAndTimezone.getTime());

    // Close the dialog.
    setTimeElement.$$('#doneButton').click();

    // The browser validates the change.
    await testBrowserProxy.whenCalled('doneClicked');
    cr.webUIListenerCallback('validation-complete');

    const timeInSeconds = await testBrowserProxy.whenCalled('setTimeInSeconds');
    const todaySeconds = originalTime.getTime() / 1000;
    // The exact value isn't important (it depends on the current time).
    // timeInSeconds should be bigger, because this timestamp is seconds
    // since epoch and it does not hold any information regarding the
    // current timezone.
    assertGT(timeInSeconds, todaySeconds);

    const newTimezone = await testBrowserProxy.whenCalled('setTimezone');
    assertEquals('America/Los_Angeles', newTimezone);
  });

  suite('HideTimezone', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({showTimezone: false});
    });

    test('SetDate', async () => {
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

      // The browser validates the change.
      await testBrowserProxy.whenCalled('doneClicked');
      cr.webUIListenerCallback('validation-complete');

      // Verify the page sends a request to move time backward.
      const newTimeSeconds =
          await testBrowserProxy.whenCalled('setTimeInSeconds');
      const todaySeconds = today.getTime() / 1000;
      // Check that the current time is bigger than the new time, which
      // is supposed to be two days ago. The exact value isn't
      // important, checking it is difficult because it depends on the
      // current time, which is constantly updated, therefore we only
      // assert that one is bigger than the other.
      assertGT(todaySeconds, newTimeSeconds);

      // Verify the page didn't try to change the timezone.
      assertEquals(0, testBrowserProxy.getCallCount('setTimezone'));
    });

    test('TimezoneUpdate', () => {
      assertEquals(null, setTimeElement.$$('#timezoneSelect'));
      cr.webUIListenerCallback(
          'system-timezone-changed', 'America/Los_Angeles');
      // No crash.
    });
  });
});

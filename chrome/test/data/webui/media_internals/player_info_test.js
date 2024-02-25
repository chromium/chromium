// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertThrows} from 'chrome://webui-test/chai_assert.js';

suite('player info test', function() {
  setup(function() {
    window.pi = new PlayerInfo('example_id');
  });

  teardown(function() {
    window.pi = null;
  });

  // Test that an ID is set correctly.
  test('constructor string id', function() {
    assertEquals('example_id', window.pi.id);
  });

  // Test that numerical IDs are valid.
  test('constructor number id', function() {
    const pi = new PlayerInfo(5);
    assertEquals(5, pi.id);
  });

  // Make sure that a new PlayerInfo has no events.
  test('empty events', function() {
    assertEquals(0, window.pi.allEvents.length);
  });

  // Check that the most recent property gets updated.
  test('add property', function() {
    const key = 'key';
    const value = 'value';
    const value2 = 'value2';

    window.pi.addProperty(0, key, value);
    assertEquals(value, window.pi.properties[key]);

    window.pi.addProperty(0, key, value2);
    assertEquals(value2, window.pi.properties[key]);
  });

  // Make sure that the first timestamp that gets sent
  // is recorded as the base timestamp.
  test('first timestamp', function() {
    const pi = new PlayerInfo('example_ID');
    const timestamp = 5000;
    pi.addProperty(timestamp, 'key', 'value');

    assertEquals(timestamp, pi.firstTimestamp_);
  });

  // Adding a property with a non-string key should
  // throw an exception.
  test('wrong key type', function() {
    const pi = new PlayerInfo('example_ID');
    assertThrows(function() {
      pi.addProperty(0, 5, 'some value');
    });
  });

  // Subsequent events should have their log offset based
  // on the first timestamp added.
  test('add property timestamp offset', function() {
    const firstTimestamp = 500;
    const secondTimestamp = 550;
    const deltaT = secondTimestamp - firstTimestamp;
    const key = 'key';
    const value = 'value';

    const pi = new PlayerInfo('example_ID');
    pi.addProperty(firstTimestamp, key, value);
    pi.addProperty(secondTimestamp, key, value);

    assertEquals(firstTimestamp, pi.firstTimestamp_);
    assertEquals(0, pi.allEvents[0].time);
    assertEquals(deltaT, pi.allEvents[1].time);
  });

  // The list of all events should be recorded in correctly.
  test('all events', function() {
    const pi = new PlayerInfo('example_ID');
    const timestamp = 50;
    const key = 'key';
    const value = 'value';
    const key2 = 'key2';
    const value2 = 'value2';

    pi.addProperty(timestamp, key, value);
    assertEquals(value, pi.allEvents[0].value);
    assertEquals(key, pi.allEvents[0].key);

    pi.addProperty(timestamp, key2, value2);
    assertEquals(value2, pi.allEvents[1].value);
    assertEquals(key2, pi.allEvents[1].key);
  });

  // Using noRecord should make it not show up in allEvents.
  test('no record', function() {
    const pi = new PlayerInfo('example_ID');
    const timestamp = 50;
    const key = 'key';
    const value = 'value';
    pi.addPropertyNoRecord(timestamp, key, value);

    assertEquals(value, pi.properties[key]);
    assertEquals(0, pi.allEvents.length);
  });
});

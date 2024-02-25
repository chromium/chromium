// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/word_streamer.js';

import {WordStreamer} from 'chrome-untrusted://compose/word_streamer.js';
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

interface Update {
  words: string[];
  isComplete: boolean;
}

suite('WordStreamer', () => {
  const mockTimer = new MockTimer();
  let updates: Update[];
  let streamer: WordStreamer;
  const msPerTick = 200;

  setup(() => {
    updates = [];
    mockTimer.install();
    streamer = new WordStreamer(
        (words, isComplete) => updates.push({words, isComplete}));
    // Tests were written assuming these constants.
    streamer.setMsPerTickForTesting(msPerTick);
    streamer.setCharsPerTickForTesting(5);
    streamer.setMsWaitBeforeCompleteForTesting(300);
  });

  teardown(() => {
    streamer.reset();
    mockTimer.uninstall();
  });

  function assertUpdatesEqual(wantUpdates: Update[]) {
    const want = JSON.stringify(wantUpdates, undefined, '  ');
    const got = JSON.stringify(updates, undefined, '  ');
    if (got !== want) {
      // Formatting of the assertEquals isn't great, so make our own.
      assertTrue(false, `Updates don't match.\ngot:  ${got}\nwant: ${want}`);
    }
  }

  test('noCalls', async () => {
    mockTimer.tick(10000);
    assertUpdatesEqual([]);
  });

  test('setTextEmpty', async () => {
    streamer.setText('', true);
    mockTimer.tick(10000);
    assertUpdatesEqual([{words: [], isComplete: true}]);
  });

  test('setTextNotCompleteLastWordNotShown', async () => {
    streamer.setText('Hello Wor', false);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
    ]);
  });

  test('setTextMultipleWords', async () => {
    streamer.setText('Hello World!', true);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
      {words: ['Hello', ' World!'], isComplete: false},
      {words: ['Hello', ' World!'], isComplete: true},
    ]);
  });

  test('setTextAppend', async () => {
    streamer.setText('Hello ', false);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
    ]);

    streamer.setText('Hello Wor', false);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
    ]);

    streamer.setText('Hello World, Mom!', true);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
      {words: ['Hello', ' World,'], isComplete: false},
      {words: ['Hello', ' World,', ' Mom!'], isComplete: false},
      {words: ['Hello', ' World,', ' Mom!'], isComplete: true},
    ]);
  });

  test('setTextChangeDisplayedText', async () => {
    streamer.setText('Hello World Today', false);
    mockTimer.tick(10000);
    streamer.setText('Hello Mom', true);
    mockTimer.tick(10000);
    assertUpdatesEqual([
      {words: ['Hello'], isComplete: false},
      {words: ['Hello', ' World'], isComplete: false},
      {words: ['Hello'], isComplete: false},
      {words: ['Hello', ' Mom'], isComplete: false},
      {words: ['Hello', ' Mom'], isComplete: true},
    ]);
  });

  test('resetPreventsFutureUpdates', async () => {
    streamer.setText('Hello World!', true);
    streamer.reset();
    mockTimer.tick(10000);
    assertUpdatesEqual([]);
  });

  test('outputRate', async () => {
    streamer.setText('1234 1234 1234 1234', true);

    mockTimer.tick(msPerTick);
    assertUpdatesEqual([
      {words: ['1234'], isComplete: false},
    ]);

    mockTimer.tick(msPerTick);
    assertUpdatesEqual([
      {words: ['1234'], isComplete: false},
      {words: ['1234', ' 1234'], isComplete: false},
    ]);
  });

  test('outputRateAfterSlowInput', async () => {
    streamer.setText('1234 ', false);

    mockTimer.tick(msPerTick);
    assertUpdatesEqual([
      {words: ['1234'], isComplete: false},
    ]);

    mockTimer.tick(10000);
    streamer.setText('1234 1234 1234 1234', true);
    mockTimer.tick(msPerTick);

    assertUpdatesEqual([
      {words: ['1234'], isComplete: false},
      {words: ['1234', ' 1234'], isComplete: false},
    ]);
  });
});

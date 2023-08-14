// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {durationToString, maybeMakePlural} from 'chrome://discards/discards.js';
import {compareTabDiscardsInfos} from 'chrome://discards/discards_tab.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('discards', function() {
  test('CompareTabDiscardsInfo', function() {
    const dummy1 = {
      title: 'title 1',
      tabUrl: 'http://urlone.com',
      visibility: 0,  // hidden
      state: 0,       // active
      isAutoDiscardable: false,
      discardCount: 0,
      utilityRank: 0,
      lastActiveSeconds: 0,
    };
    const dummy2 = {
      title: 'title 2',
      tabUrl: 'http://urltwo.com',
      visibility: 1,  // occluded
      state: 3,       // frozen
      isAutoDiscardable: true,
      discardCount: 1,
      utilityRank: 1,
      lastActiveSeconds: 1,
    };

    ['title', 'tabUrl', 'visibility', 'state', 'isAutoDiscardable',
     'discardCount', 'utilityRank', 'lastActiveSeconds']
        .forEach((sortKey) => {
          assertTrue(compareTabDiscardsInfos(sortKey, dummy1, dummy2) < 0);
          assertTrue(compareTabDiscardsInfos(sortKey, dummy2, dummy1) > 0);
          assertTrue(compareTabDiscardsInfos(sortKey, dummy1, dummy1) === 0);
          assertTrue(compareTabDiscardsInfos(sortKey, dummy2, dummy2) === 0);
        });
  });

  test('DurationToString', function() {
    // Test cases have the form [ 'expected output', input_in_seconds ].
    ([
      ['just now', 0],
      ['just now', 10],
      ['just now', 59],
      ['1 minute ago', 60],
      ['10 minutes ago', 10 * 60 + 30],
      ['59 minutes ago', 59 * 60 + 59],
      ['1 hour ago', 60 * 60],
      ['1 hour and 1 minute ago', 61 * 60],
      ['1 hour and 10 minutes ago', 70 * 60 + 30],
      ['1 day ago', 24 * 60 * 60],
      ['2 days ago', 2.5 * 24 * 60 * 60],
      ['6 days ago', 6.9 * 24 * 60 * 60],
      ['over 1 week ago', 7 * 24 * 60 * 60],
      ['over 2 weeks ago', 2.5 * 7 * 24 * 60 * 60],
      ['over 4 weeks ago', 30 * 24 * 60 * 60],
      ['over 1 month ago', 30.5 * 24 * 60 * 60],
      ['over 2 months ago', 2.5 * 30.5 * 24 * 60 * 60],
      ['over 11 months ago', 364 * 24 * 60 * 60],
      ['over 1 year ago', 365 * 24 * 60 * 60],
      ['over 2 years ago', 2.3 * 365 * 24 * 60 * 60],
    ] as Array<[string, number]>)
        .forEach((data) => {
          assertEquals(data[0], durationToString(data[1]));
        });
  });

  test('MaybeMakePlural', function() {
    assertEquals('hours', maybeMakePlural('hour', 0));
    assertEquals('hour', maybeMakePlural('hour', 1));
    assertEquals('hours', maybeMakePlural('hour', 2));
    assertEquals('hours', maybeMakePlural('hour', 10));
  });
});

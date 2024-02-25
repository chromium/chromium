// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('manager tests', function() {
  const doNothing = function() {};
  const emptyClientRenderer = {
    playerAdded: doNothing,
    playerRemoved: doNothing,
    playerUpdated: doNothing,
  };

  setup(function() {
    window.pm = new Manager(emptyClientRenderer);
  });

  teardown(function() {
    window.pm = null;
  });

  // Test a normal case of .addPlayer
  test('add player', function() {
    window.pm.addPlayer('someid');
    assertTrue(undefined !== window.pm.players_['someid']);
  });

  // On occasion, the backend will add an existing ID multiple times.
  // make sure this doesn't break anything.
  test('add player already existing', function() {
    window.pm.addPlayer('someid');
    window.pm.addPlayer('someid');
    assertTrue(undefined !== window.pm.players_['someid']);
  });

  // If the removal is set, make sure that a player
  // gets removed from the PlayerManager.
  test('remove player', function() {
    window.pm.addPlayer('someid');
    assertTrue(undefined !== window.pm.players_['someid']);
    window.pm.removePlayer('someid');
    assertTrue(undefined === window.pm.players_['someid']);
  });

  // Trying to select a non-existent player should throw
  // an exception
  test('select non existent', function() {
    assertThrows(function() {
      window.pm.selectPlayer('someId');
    });
  });
});

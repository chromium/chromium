// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Histograms WebUI.
 */

GEN('#include "base/metrics/histogram.h"');
GEN('#include "content/public/test/browser_test.h"');

function MediaInternalsUIBrowserTest() {}

MediaInternalsUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-internals',

  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('MediaInternalsUIBrowserTest', 'Integration', function() {
  suite('integration_tests', function() {
    // The renderer and player ids are completely arbitrarily.
    var TEST_RENDERER = 12;
    var TEST_PLAYER = 4;
    var TEST_NAME = TEST_RENDERER + ':' + TEST_PLAYER;

    setup(function() {
      var doNothing = function() {};
      var mockClientRenderer = {
        playerUpdated: doNothing,
        playerRemoved: doNothing,
        playerAdded: doNothing,
        audioComponentAdded: doNothing,
        audioComponentRemoved: doNothing,
        generalAudioInformationSet: doNothing,
        redrawVideoCaptureCapabilities: doNothing,
        audioFocusSessionUpdated: doNothing,
        updateRegisteredCdms: doNothing,
      };

      var manager = new Manager(mockClientRenderer);
      initialize(manager);

      window.manager = manager;
    });

    test('on media event', function() {
      // Correctly use the information from a media event.
      var event = {
        ticksMillis: 132,
        renderer: TEST_RENDERER,
        player: TEST_PLAYER,
        params: {fps: 60, other: 'hi'},
      };

      window.media.onMediaEvent(event);
      var info = window.manager.players_[TEST_NAME];

      assertEquals(event.ticksMillis, info.firstTimestamp_);
      assertEquals(TEST_NAME, info.id);
      assertEquals(event.params.fps, info.properties.fps);
    });

    test('audio components', function() {
      var event =
          {component_id: 1, component_type: 0, owner_id: 3, status: 'created'};

      // Ensure no components are currently present.
      assertEquals(0, window.manager.audioComponents_.length);

      // Test adding an audio component.
      window.media.updateAudioComponent(event);
      assertEquals(1, window.manager.audioComponents_.length);

      // The key format is an implementation detail we don't care about, so
      // just ensure there's only one key and then use it directly.
      assertEquals(
          1,
          Object.keys(window.manager.audioComponents_[event.component_type])
              .length);
      for (key in window.manager.audioComponents_[event.component_type]) {
        var component =
            window.manager.audioComponents_[event.component_type][key];
        assertEquals(event.component_id, component['component_id']);
        assertEquals(event.component_type, component['component_type']);
        assertEquals(event.owner_id, component['owner_id']);
        assertEquals(event.status, component['status']);
      }

      // Test removing an audio component.
      event.status = 'closed';
      window.media.updateAudioComponent(event);
      assertEquals(1, window.manager.audioComponents_.length);
      assertEquals(
          0,
          Object.keys(window.manager.audioComponents_[event.component_type])
              .length);
    });

    test('general audio information', function() {
      var audioInfo = {property: 'value'};
      window.media.updateGeneralAudioInformation(audioInfo);
      assertEquals(audioInfo.property, window.manager.audioInfo_.property);
    });
  });

  mocha.run();
});

TEST_F('MediaInternalsUIBrowserTest', 'Manager', function() {
  suite('manager tests', function() {
    var doNothing = function() {};
    var emptyClientRenderer = {
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

  mocha.run();
});

TEST_F('MediaInternalsUIBrowserTest', 'PlayerInfo', function() {
  test('player info test', function() {
    suiteSetup(function() {
      window.chrome = {};
    });

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
      var pi = new PlayerInfo(5);
      assertEquals(5, pi.id);
    });

    // Make sure that a new PlayerInfo has no events.
    test('empty events', function() {
      assertEquals(0, window.pi.allEvents.length);
    });

    // Check that the most recent property gets updated.
    test('add property', function() {
      var key = 'key', value = 'value', value2 = 'value2';

      window.pi.addProperty(0, key, value);
      assertEquals(value, window.pi.properties[key]);

      window.pi.addProperty(0, key, value2);
      assertEquals(value2, window.pi.properties[key]);
    });

    // Make sure that the first timestamp that gets sent
    // is recorded as the base timestamp.
    test('first timestamp', function() {
      var pi = new PlayerInfo('example_ID');
      var timestamp = 5000;
      pi.addProperty(timestamp, 'key', 'value');

      assertEquals(timestamp, pi.firstTimestamp_);
    });

    // Adding a property with a non-string key should
    // throw an exception.
    test('wrong key type', function() {
      var pi = new PlayerInfo('example_ID');
      assertThrows(function() {
        pi.addProperty(0, 5, 'some value');
      });
    });

    // Subsequent events should have their log offset based
    // on the first timestamp added.
    test('add property timestamp offset', function() {
      var firstTimestamp = 500, secondTimestamp = 550,
          deltaT = secondTimestamp - firstTimestamp, key = 'key',
          value = 'value';

      var pi = new PlayerInfo('example_ID');
      pi.addProperty(firstTimestamp, key, value);
      pi.addProperty(secondTimestamp, key, value);

      assertEquals(firstTimestamp, pi.firstTimestamp_);
      assertEquals(0, pi.allEvents[0].time);
      assertEquals(deltaT, pi.allEvents[1].time);
    });

    // The list of all events should be recorded in correctly.
    test('all events', function() {
      var pi = new PlayerInfo('example_ID'), timestamp = 50, key = 'key',
          value = 'value', key2 = 'key2', value2 = 'value2';

      pi.addProperty(timestamp, key, value);
      assertEquals(value, pi.allEvents[0].value);
      assertEquals(key, pi.allEvents[0].key);

      pi.addProperty(timestamp, key2, value2);
      assertEquals(value2, pi.allEvents[1].value);
      assertEquals(key2, pi.allEvents[1].key);
    });

    // Using noRecord should make it not show up in allEvents.
    test('no record', function() {
      var pi = new PlayerInfo('example_ID'), timestamp = 50, key = 'key',
          value = 'value';
      pi.addPropertyNoRecord(timestamp, key, value);

      assertEquals(value, pi.properties[key]);
      assertEquals(0, pi.allEvents.length);
    });
  });

  mocha.run();
});

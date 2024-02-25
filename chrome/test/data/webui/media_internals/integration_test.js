// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('integration_tests', function() {
  // The renderer and player ids are completely arbitrarily.
  const TEST_RENDERER = 12;
  const TEST_PLAYER = 4;
  const TEST_NAME = TEST_RENDERER + ':' + TEST_PLAYER;

  setup(function() {
    const doNothing = function() {};
    const mockClientRenderer = {
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

    const manager = new Manager(mockClientRenderer);
    initialize(manager);

    window.manager = manager;
  });

  test('on media event', function() {
    // Correctly use the information from a media event.
    const event = {
      ticksMillis: 132,
      renderer: TEST_RENDERER,
      player: TEST_PLAYER,
      params: {fps: 60, other: 'hi'},
    };

    window.media.onMediaEvent(event);
    const info = window.manager.players_[TEST_NAME];

    assertEquals(event.ticksMillis, info.firstTimestamp_);
    assertEquals(TEST_NAME, info.id);
    assertEquals(event.params.fps, info.properties.fps);
  });

  test('audio components', function() {
    const event =
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
    for (const key in window.manager.audioComponents_[event.component_type]) {
      const component =
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
    const audioInfo = {property: 'value'};
    window.media.updateGeneralAudioInformation(audioInfo);
    assertEquals(audioInfo.property, window.manager.audioInfo_.property);
  });
});

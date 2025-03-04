// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {IMAGES_TOGGLE_BUTTON_ID} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, setDefaultSpeechSynthesis} from './common.js';
import type {FakeSpeechSynthesis} from './fake_speech_synthesis.js';


suite('Images', () => {
  let app: AppElement;
  let imagesToggleButton: CrIconButtonElement|null;

  function setTree(rootChildren: number[], nodes: Object[]) {
    const tree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: rootChildren,
        },
        ...nodes,
      ],
    };

    chrome.readingMode.setContentForTesting(tree, rootChildren);
  }

  function assertHtml(expected: string) {
    assertEquals(expected, app.$.container.innerHTML);
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};

    // Override chrome.readingMode.requestImageData to avoid the cross-process
    // hop.
    chrome.readingMode.requestImageData = (nodeId: number) => {
      chrome.readingMode.onImageDownloaded(nodeId);
    };

    app = await createApp();
    assertTrue(chrome.readingMode.imagesFeatureEnabled);
    imagesToggleButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#' + IMAGES_TOGGLE_BUTTON_ID);
    assertTrue(!!imagesToggleButton);
    if (!chrome.readingMode.imagesEnabled) {
      imagesToggleButton.click();
      return microtasksFinished();
    }
  });

  test('image', () => {
    chrome.readingMode.getImageBitmap = (_: number) => {
      return {
        data: new Uint8ClampedArray(),
        width: 30,
        height: 40,
        scale: 0.5,
      };
    };

    const nodes = [
      {
        id: 2,
        role: 'image',
        htmlTag: 'img',
      },
    ];
    const expected = '<div><canvas alt="" class="downloaded-image" width="30"' +
        ' height="40" style="zoom: 0.5;"></canvas></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });

  test('figure with caption', () => {
    chrome.readingMode.getImageBitmap = (_: number) => {
      return {
        data: new Uint8ClampedArray(),
        width: 30,
        height: 40,
        scale: 0.5,
      };
    };

    const nodes = [
      {
        id: 2,
        role: 'figure',
        htmlTag: 'figure',
        childIds: [3, 4],
      },
      {
        id: 3,
        role: 'image',
        htmlTag: 'canvas',
      },
      {
        id: 4,
        role: 'figcaption',
        htmlTag: 'figcaption',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'Statistics and numbers and things',
      },
    ];
    const expected = '<div><figure><canvas alt="" class="downloaded-image" ' +
        'width="30" height="40" style="zoom: 0.5;"></canvas><figcaption>' +
        'Statistics and numbers and things</figcaption></figure></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });

  suite('with read aloud,', () => {
    let speechSynthesis: FakeSpeechSynthesis;

    setup(() => {
      speechSynthesis = setDefaultSpeechSynthesis(app);
    });

    test('image captions are read aloud when showing', () => {
      chrome.readingMode.getImageBitmap = (_: number) => {
        return {
          data: new Uint8ClampedArray(),
          width: 30,
          height: 40,
          scale: 0.5,
        };
      };
      const figcaption = 'Statistics and numbers and things';
      const nodes = [
        {
          id: 2,
          role: 'figure',
          htmlTag: 'figure',
          childIds: [3, 4],
        },
        {
          id: 3,
          role: 'image',
          htmlTag: 'canvas',
        },
        {
          id: 4,
          role: 'figcaption',
          htmlTag: 'figcaption',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: figcaption,
        },
      ];
      setTree([2], nodes);

      app.playSpeech();

      assertEquals(1, speechSynthesis.spokenUtterances.length);
      assertEquals(figcaption, speechSynthesis.spokenUtterances[0]!.text);
    });

    test('image captions are not read aloud when hidden', async () => {
      chrome.readingMode.getImageBitmap = (_: number) => {
        return {
          data: new Uint8ClampedArray(),
          width: 30,
          height: 40,
          scale: 0.5,
        };
      };
      const figcaption = 'Statistics and numbers and things';
      const nodes = [
        {
          id: 2,
          role: 'figure',
          htmlTag: 'figure',
          childIds: [3, 4],
        },
        {
          id: 3,
          role: 'image',
          htmlTag: 'canvas',
        },
        {
          id: 4,
          role: 'figcaption',
          htmlTag: 'figcaption',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: figcaption,
        },
      ];
      setTree([2], nodes);
      imagesToggleButton!.click();
      assertFalse(chrome.readingMode.imagesEnabled);
      await microtasksFinished();

      app.playSpeech();

      assertEquals(0, speechSynthesis.spokenUtterances.length);
    });
  });
});

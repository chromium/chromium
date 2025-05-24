// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {LINK_TOGGLE_BUTTON_ID, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, emitEvent, setupBasicSpeech} from './common.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('LinksToggledIntegration', () => {
  let app: AppElement;
  let linksToggleButton: CrIconButtonElement|null;
  let speechController: SpeechController;

  // root htmlTag='#document' id=1
  // ++link htmlTag='a' url='http://www.google.com' id=2
  // ++++staticText name='This is a link.' id=3
  // ++link htmlTag='a' url='http://www.youtube.com' id=4
  // ++++staticText name='This is another link.' id=5
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        // The space at the end is needed so that we can parse two separate
        // sentences.
        name: 'This is a link. ',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.youtube.com',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is another link.',
      },
    ],
  };

  function hasLinks() {
    return !!(app.$.container.querySelector('a[href]'));
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};
    const speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);

    app = await createApp();
    linksToggleButton =
        app.$.toolbar.shadowRoot.querySelector<CrIconButtonElement>(
            '#' + LINK_TOGGLE_BUTTON_ID);
    assertTrue(!!linksToggleButton);
    chrome.readingMode.setContentForTesting(axTree, [3, 5]);
    setupBasicSpeech(speech);
  });

  test('container has links by default', () => {
    assertTrue(hasLinks());
  });

  test('container has no highlight by default', () => {
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertFalse(!!currentHighlight);
  });

  test('on first toggle, links are disabled', async () => {
    linksToggleButton!.click();
    await microtasksFinished();
    assertFalse(hasLinks());
  });

  test('on next toggle, links are enabled', async () => {
    linksToggleButton!.click();
    await microtasksFinished();
    assertTrue(hasLinks());
  });

  suite('after speech starts', () => {
    setup(() => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    });

    test('container does not have links', () => {
      assertFalse(hasLinks());
    });

    test('container has highlight', () => {
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight);
    });


    test('and after speech finishes, container has links again', async () => {
      for (let i = 0; i < axTree.nodes.length + 1; i++) {
        emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
      }
      await microtasksFinished();
      assertTrue(hasLinks());
    });
  });

  suite('after speech pauses', () => {
    setup(() => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    });

    test('container has links again', () => {
      assertTrue(hasLinks());
    });

    test('container still has highlight', () => {
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight);
    });
  });

  suite('with links toggled off', () => {
    setup(() => {
      // Only toggle once otherwise we'll keep enabling and disabling links
      // across tests
      if (chrome.readingMode.linksEnabled) {
        linksToggleButton!.click();
      }
    });

    test('container does not have links', () => {
      assertFalse(hasLinks());
    });

    suite('after speech starts', () => {
      setup(() => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      });

      test('container does not have links', () => {
        assertFalse(hasLinks());
      });

      test('container has highlight', () => {
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight);
      });
    });

    suite('after speech pauses', () => {
      setup(() => {
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
        emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      });

      test('container does not have links', () => {
        assertFalse(hasLinks());
      });

      test('container still has highlight', () => {
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight);
      });
    });
  });
});

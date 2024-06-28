// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {LINK_TOGGLE_BUTTON_ID, PauseActionSource, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, emitEvent, suppressInnocuousErrors} from './common.js';

suite('LinksToggledIntegration', () => {
  let app: ReadAnythingElement;
  let linksToggleButton: CrIconButtonElement;

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
        name: 'This is a link.',
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

  function assertContainerHasLinks(hasLinks: boolean) {
    const innerHTML = app.$.container.innerHTML;
    assertEquals(hasLinks, innerHTML.includes('a href'));
  }

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    flush();
    linksToggleButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#' + LINK_TOGGLE_BUTTON_ID)!;
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
    app.enabledLangs = ['en-US'];
    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Kristi'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
  });

  suite('by default', () => {
    test('container has links', () => {
      assertContainerHasLinks(true);
    });

    test('container has no highlight', () => {
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertFalse(!!currentHighlight);
    });
  });

  suite('without speech', () => {
    test('on first toggle, links are disabled', () => {
      linksToggleButton.click();
      assertContainerHasLinks(false);
    });

    test('on next toggle, links are enabled', () => {
      linksToggleButton.click();
      assertContainerHasLinks(true);
    });
  });

  suite('after speech starts', () => {
    setup(() => {
      app.playSpeech();
    });

    test('container does not have links', () => {
      assertContainerHasLinks(false);
    });

    test('container has highlight', () => {
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight);
    });

    suite('and after speech finishes', () => {
      setup(() => {
        for (let i = 0; i < axTree.nodes.length + 1; i++) {
          emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
        }
      });

      test('container has links again', () => {
        assertContainerHasLinks(true);
      });
    });
  });

  suite('after speech pauses', () => {
    setup(() => {
      app.playSpeech();
      app.stopSpeech(PauseActionSource.BUTTON_CLICK);
    });

    test('container has links again', () => {
      assertContainerHasLinks(true);
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
        linksToggleButton.click();
      }
    });

    test('container does not have links', () => {
      assertContainerHasLinks(false);
    });

    suite('after speech starts', () => {
      setup(() => {
        app.playSpeech();
      });

      test('container does not have links', () => {
        assertContainerHasLinks(false);
      });

      test('container has highlight', () => {
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight);
      });
    });

    suite('after speech pauses', () => {
      setup(() => {
        app.playSpeech();
        app.stopSpeech(PauseActionSource.BUTTON_CLICK);
      });

      test('container does not have links', () => {
        assertContainerHasLinks(false);
      });

      test('container still has highlight', () => {
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight);
      });
    });
  });
});

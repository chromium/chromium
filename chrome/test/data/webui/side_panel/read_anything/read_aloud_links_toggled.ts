// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('ReadAloudLinksToggled', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let linksToggleButton: CrIconButtonElement;
  let playPauseButton: CrIconButtonElement;

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


  /**
   * Suppresses harmless ResizeObserver errors due to a browser bug.
   * yaqs/2300708289911980032
   */
  function suppressInnocuousErrors() {
    const onerror = window.onerror;
    window.onerror = (message, url, lineNumber, column, error) => {
      if ([
            'ResizeObserver loop limit exceeded',
            'ResizeObserver loop completed with undelivered notifications.',
          ].includes(message.toString())) {
        console.info('Suppressed ResizeObserver error: ', message);
        return;
      }
      if (onerror) {
        onerror.apply(window, [message, url, lineNumber, column, error]);
      }
    };
  }

  function assertContainerHasLinks(hasLinks: boolean) {
    const innerHTML = app.$.container.innerHTML;
    assertEquals(innerHTML.includes('a href'), hasLinks);
  }

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    linksToggleButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#' + LINK_TOGGLE_BUTTON_ID)!;
    playPauseButton =
        app.$.toolbar.shadowRoot!.querySelector<CrIconButtonElement>(
            '#play-pause')!;
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  suite('by default', () => {
    test('link toggle enabled', () => {
      assertFalse(linksToggleButton.disabled);
    });

    test('links are enabled', () => {
      assertEquals(linksToggleButton.ironIcon, LINKS_ENABLED_ICON);
    });

    test('container has links', () => {
      assertContainerHasLinks(true);
    });
  });

  suite('after speech starts', () => {
    setup(() => {
      playPauseButton.click();
    });

    test('link toggle disabled', () => {
      assertTrue(linksToggleButton.disabled);
    });

    test('links are enabled', () => {
      assertEquals(linksToggleButton.ironIcon, LINKS_ENABLED_ICON);
    });

    test('container does not have links', () => {
      assertContainerHasLinks(false);
    });

    suite('and after speech finishes', () => {
      setup(() => {
        for (let i = 0; i < axTree.nodes.length + 1; i++) {
          app.playNextGranularity();
        }
      });

      test('link toggle enabled', () => {
        assertFalse(linksToggleButton.disabled);
      });

      test('links are enabled', () => {
        assertEquals(linksToggleButton.ironIcon, LINKS_ENABLED_ICON);
      });

      test('container has links again', () => {
        assertContainerHasLinks(true);
      });
    });
  });

  suite('after speech pauses', () => {
    setup(() => {
      playPauseButton.click();
      playPauseButton.click();
    });

    test('link toggle enabled', () => {
      assertFalse(linksToggleButton.disabled);
    });

    test('links are enabled', () => {
      assertEquals(linksToggleButton.ironIcon, LINKS_ENABLED_ICON);
    });

    test('container has links again', () => {
      assertContainerHasLinks(true);
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

    test('link toggle enabled', () => {
      assertFalse(linksToggleButton.disabled);
    });

    test('links are disabled', () => {
      assertEquals(linksToggleButton.ironIcon, LINKS_DISABLED_ICON);
    });

    test('container does not have links', () => {
      assertContainerHasLinks(false);
    });

    suite('after speech starts', () => {
      setup(() => {
        playPauseButton.click();
      });

      test('link toggle disabled', () => {
        assertTrue(linksToggleButton.disabled);
      });

      test('links are disabled', () => {
        assertEquals(linksToggleButton.ironIcon, LINKS_DISABLED_ICON);
      });

      test('container does not have links', () => {
        assertContainerHasLinks(false);
      });
    });

    suite('after speech pauses', () => {
      setup(() => {
        playPauseButton.click();
        playPauseButton.click();
      });

      test('link toggle enabled', () => {
        assertFalse(linksToggleButton.disabled);
      });

      test('links are disabled', () => {
        assertEquals(linksToggleButton.ironIcon, LINKS_DISABLED_ICON);
      });

      test('container does not have links', () => {
        assertContainerHasLinks(false);
      });
    });
  });
});

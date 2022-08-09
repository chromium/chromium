// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/read_anything/app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ReadAnythingElement} from 'chrome://read-later.top-chrome/read_anything/app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ReadAnythingAppTest', () => {
  let readAnythingApp: ReadAnythingElement;

  // Do not call the real `onConnected()`. As defined in
  // ReadAnythingAppController, onConnected creates mojo pipes to connect to the
  // rest of the Read Anything feature, which we are not testing here.
  chrome.readAnything.onConnected = function() {};

  setup(() => {
    document.body.innerHTML = '';
    readAnythingApp = document.createElement('read-anything-app');
    document.body.appendChild(readAnythingApp);
    chrome.readAnything.setThemeForTesting('default', 18.0);
  });

  function assertFontName(fontFamily: string) {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(fontFamily, getComputedStyle(container!).fontFamily);
  }

  function assertFontSize(fontSize: string) {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(fontSize, getComputedStyle(container!).fontSize);
  }

  function assertContainerInnerHTML(expected: string) {
    const actual: string =
        readAnythingApp.shadowRoot!.getElementById('container')!.innerHTML;
    assertEquals(actual, expected);
  }

  test('updateTheme fontName', () => {
    chrome.readAnything.setThemeForTesting('Standard font', 18.0);
    assertFontName('"Standard font"');

    chrome.readAnything.setThemeForTesting('Sans-serif', 18.0);
    assertFontName('sans-serif');

    chrome.readAnything.setThemeForTesting('Serif', 18.0);
    assertFontName('serif');

    chrome.readAnything.setThemeForTesting('Avenir', 18.0);
    assertFontName('avenir');

    chrome.readAnything.setThemeForTesting('Comic Neue', 18.0);
    assertFontName('"Comic Neue"');

    chrome.readAnything.setThemeForTesting('Comic Sans MS', 18.0);
    assertFontName('"Comic Sans MS"');

    chrome.readAnything.setThemeForTesting('Poppins', 18.0);
    assertFontName('poppins');
  });

  test('updateTheme fontSize', () => {
    chrome.readAnything.setThemeForTesting('Standard font', 27.0);
    assertFontSize('27px');
  });

  test('updateContent paragraph', () => {
    // root id=1
    // ++paragraph id=2
    // ++++staticText name='This is a paragraph' id=3
    // ++paragraph id=4
    // ++++staticText name='This is a second paragraph' id=5
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2, 4],
        },
        {
          id: 2,
          role: 'paragraph',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is a paragraph',
        },
        {
          id: 4,
          role: 'paragraph',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This is a second paragraph',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected: string =
        '<p>This is a paragraph</p><p>This is a second paragraph</p>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent heading', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++heading hierarchicalLevel=1 id=2
    // ++++staticText name='This is an h1.' id=3
    // ++heading hierarchicalLevel=2 id=4
    // ++++staticText name='This is an h2.' id=5
    // ++heading hierarchicalLevel=3 id=6
    // ++++staticText name='This is an h3.' id=7
    // ++heading hierarchicalLevel=4 id=8
    // ++++staticText name='This is an h4.' id=9
    // ++heading hierarchicalLevel=5 id=10
    // ++++staticText name='This is an h5.' id=11
    // ++heading hierarchicalLevel=6 id=12
    // ++++staticText name='This is an h6.' id=13
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2, 4, 6, 8, 10, 12],
        },
        {
          id: 2,
          role: 'heading',
          hierarchicalLevel: 1,
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is an h1.',
        },
        {
          id: 4,
          role: 'heading',
          hierarchicalLevel: 12,
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This is an h2.',
        },
        {
          id: 6,
          role: 'heading',
          hierarchicalLevel: 3,
          childIds: [7],
        },
        {
          id: 7,
          role: 'staticText',
          name: 'This is an h3.',
        },
        {
          id: 8,
          role: 'heading',
          hierarchicalLevel: 4,
          childIds: [9],
        },
        {
          id: 9,
          role: 'staticText',
          name: 'This is an h4.',
        },
        {
          id: 10,
          role: 'heading',
          hierarchicalLevel: 5,
          childIds: [11],
        },
        {
          id: 11,
          role: 'staticText',
          name: 'This is an h5.',
        },
        {
          id: 12,
          role: 'heading',
          hierarchicalLevel: 6,
          childIds: [13],
        },
        {
          id: 13,
          role: 'staticText',
          name: 'This is an h6.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4, 6, 8, 10, 12]);
    const expected: string =
        '<h1 align="left">This is an h1.</h1><h2 align="left">This is an h2.</h2><h3 align="left">This is an h3.</h3><h4 align="left">This is an h4.</h4><h5 align="left">This is an h5.</h5><h6 align="left">This is an h6.</h6>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent heading badInput', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++heading hierarchicalLevel=0 id=2
    // ++++staticText name='This is a heading with an improper level.' id=3
    // ++heading hierarchicalLevel=7 id=4
    // ++++staticText name='This also has an improper heading level.' id=5
    // ++heading id=6
    // ++++staticText name='This heading has no level specified.' id=7
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2, 4, 6],
        },
        {
          id: 2,
          role: 'heading',
          hierarchicalLevel: 0,
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is a heading with an improper level.',
        },
        {
          id: 4,
          role: 'heading',
          hierarchicalLevel: 7,
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This also has an improper heading level.',
        },
        {
          id: 6,
          role: 'heading',
          childIds: [7],
        },
        {
          id: 7,
          role: 'staticText',
          name: 'This heading has no level specified.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4, 6]);
    const expected: string =
        '<h2 align="left">This is a heading with an improper level.</h2><h2 align="left">This also has an improper heading level.</h2><h2 align="left">This heading has no level specified.</h2>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent link', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++link url='http://www.google.com' id=2
    // ++++staticText name='This is a link.' id=3
    // ++link url='http://www.youtube.com' id=4
    // ++++staticText name='This is another link.' id=5
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2, 4],
        },
        {
          id: 2,
          role: 'link',
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
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected: string =
        '<a href="http://www.google.com">This is a link.</a><a href="http://www.youtube.com">This is another link.</a>';
    assertContainerInnerHTML(expected);
  });

  // Links must have a url.
  test('updateContent link badInput', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++link id=2
    // ++++staticText name='This link does not have a url.' id=3
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2],
        },
        {
          id: 2,
          role: 'link',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This link does not have a url.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    assertContainerInnerHTML('');
  });

  test('updateContent staticText', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++staticText name='This is some text.' id=2
    // ++staticText name='This is some more text.' id=3
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2, 3],
        },
        {
          id: 2,
          role: 'staticText',
          name: 'This is some text.',
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is some more text.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 3]);
    const expected: string = 'This is some text.This is some more text.';
    assertContainerInnerHTML(expected);
  });

  test('updateContent staticText badInput', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++staticText name='' id=2
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2],
        },
        {
          id: 2,
          role: 'staticText',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    assertContainerInnerHTML('');
  });

  // The container clears its old content when it receives new content.
  test('updateContent clearContainer', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++staticText name='First set of content.' id=2
    const axTree1 = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2],
        },
        {
          id: 2,
          role: 'staticText',
          name: 'First set of content.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree1, [2]);
    const expected1: string = 'First set of content.';
    assertContainerInnerHTML(expected1);

    // Fake chrome.readAnything methods for the following AXTree
    // root id=1
    // ++staticText name='Second set of content.' id=2
    const axTree2 = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          childIds: [2],
        },
        {
          id: 2,
          role: 'staticText',
          name: 'Second set of content.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree2, [2]);
    const expected2: string = 'Second set of content.';
    assertContainerInnerHTML(expected2);
  });
});

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-anything-side-panel.top-chrome/app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ReadAnythingElement} from 'chrome://read-anything-side-panel.top-chrome/app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ReadAnythingAppTest', () => {
  let readAnythingApp: ReadAnythingElement;

  // Do not call the real `onConnected()`. As defined in
  // ReadAnythingAppController, onConnected creates mojo pipes to connect to the
  // rest of the Read Anything feature, which we are not testing here.
  chrome.readAnything.onConnected = () => {};

  // This is called by readAnythingApp onselectionchange. It is usually
  // implemented by ReadAnythingAppController which forwards these arguments to
  // the browser process in the form of an AXEventNotificationDetail. Instead,
  // we capture the arguments here and verify their values. Since
  // onselectionchange is called asynchronously, the test must wait for this
  // function to be called; therefore we fire a custom event
  // on-selection-change-for-text here for the test to await.
  chrome.readAnything.onSelectionChange =
      (anchorNodeId: number, anchorOffset: number, focusNodeId: number,
       focusOffset: number) => {
        readAnythingApp.shadowRoot!.dispatchEvent(
            new CustomEvent('on-selection-change-for-test', {
              detail: {
                anchorNodeId: anchorNodeId,
                anchorOffset: anchorOffset,
                focusNodeId: focusNodeId,
                focusOffset: focusOffset,
              },
            }));
      };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    readAnythingApp = document.createElement('read-anything-app');
    document.body.appendChild(readAnythingApp);
    chrome.readAnything.setThemeForTesting('default', 18.0, 0, 0, 1, 0);
  });

  const assertFontName = (fontFamily: string) => {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(fontFamily, getComputedStyle(container!).fontFamily);
  };

  const assertFontSize = (fontSize: string) => {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(fontSize, getComputedStyle(container!).fontSize);
  };

  const assertLineSpacing = (lineSpacing: string) => {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(lineSpacing, getComputedStyle(container!).lineHeight);
  };

  const assertContainerInnerHTML = (expected: string) => {
    const actual =
        readAnythingApp.shadowRoot!.getElementById('container')!.innerHTML;
    assertEquals(actual, expected);
  };

  test('updateTheme fontName', () => {
    chrome.readAnything.setThemeForTesting('Standard font', 18.0, 0, 0, 1, 0);
    assertFontName('"Standard font"');

    chrome.readAnything.setThemeForTesting('Sans-serif', 18.0, 0, 0, 1, 0);
    assertFontName('sans-serif');

    chrome.readAnything.setThemeForTesting('Serif', 18.0, 0, 0, 1, 0);
    assertFontName('serif');

    chrome.readAnything.setThemeForTesting('Arial', 18.0, 0, 0, 1, 0);
    assertFontName('Arial');

    chrome.readAnything.setThemeForTesting('Comic Sans MS', 18.0, 0, 0, 1, 0);
    assertFontName('"Comic Sans MS"');

    chrome.readAnything.setThemeForTesting('Times New Roman', 18.0, 0, 0, 1, 0);
    assertFontName('"Times New Roman"');
  });

  test('updateTheme fontSize', () => {
    chrome.readAnything.setThemeForTesting('Standard font', 1.0, 0, 0, 1, 0);
    assertFontSize('16px');  // 1em = 16px
  });

  test('updateTheme foregroundColor', () => {
    chrome.readAnything.setThemeForTesting(
        'f', 1, /* SkColorSetRGB(0x33, 0x36, 0x39) = */ 4281546297, 0, 1, 0);
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(
        /* #333639 = */ 'rgb(51, 54, 57)', getComputedStyle(container!).color);
  });

  test('updateTheme backgroundColor', () => {
    chrome.readAnything.setThemeForTesting(
        'f', 1, 0, /* SkColorSetRGB(0xFD, 0xE2, 0x93) = */ 4294828691, 1, 0);
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(
        /* #FDE293 = */ 'rgb(253, 226, 147)',
        getComputedStyle(container!).backgroundColor);
  });

  test('updateTheme lineSpacing', () => {
    chrome.readAnything.setThemeForTesting('Standard font', 1.0, 0, 0, 2, 0);
    assertLineSpacing('24px');  // 1.5 times the 1em (16px) font size
  });

  test('updateTheme letterSpacing', () => {
    chrome.readAnything.setThemeForTesting('f', 1, 0, 0, 1, 3);
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    // very loose letter letter spacing = 0.1em, font size = 1em = 16px
    assertEquals('1.6px', getComputedStyle(container!).letterSpacing);
  });

  test('updateContent paragraph', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='This is a paragraph' id=3
    // ++paragraph htmlTag='p' id=4
    // ++++staticText name='This is a second paragraph' id=5
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
          role: 'paragraph',
          htmlTag: 'p',
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
          htmlTag: 'p',
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
    const expected = '<div><p>This is a paragraph</p>' +
        '<p>This is a second paragraph</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent language childNodeDiffLang', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2 language='en'
    // ++++staticText name='This is in English' id=3
    // ++paragraph htmlTag='p' id=4 language='es'
    // ++++staticText name='Esto es en español' id=5
    // ++++link htmlTag='a' url='http://www.google.cn/' id=6 language='zh'
    // ++++++staticText name='This is a link in Chinese' id=7
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
          role: 'paragraph',
          htmlTag: 'p',
          language: 'en',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is in English',
        },
        {
          id: 4,
          role: 'paragraph',
          htmlTag: 'p',
          language: 'es',
          childIds: [5, 6],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'Esto es en español',
        },
        {
          id: 6,
          role: 'link',
          htmlTag: 'a',
          language: 'zh',
          url: 'http://www.google.cn/',
          childIds: [7],
        },
        {
          id: 7,
          role: 'staticText',
          name: 'This is a link in Chinese',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><p lang="en">This is in English</p>' +
        '<p lang="es">Esto es en español' +
        '<a href="http://www.google.cn/" lang="zh">' +
        'This is a link in Chinese</a></p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent language parentLangSet', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2 language='en'
    // ++++staticText name='This is in English' id=3
    // ++++link htmlTag='a' url='http://www.google.com/' id=4
    // ++++++staticText name='This link has no language set' id=5
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          language: 'en',
          childIds: [3, 4],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is in English',
        },
        {
          id: 4,
          role: 'link',
          htmlTag: 'a',
          url: 'http://www.google.com/',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This link has no language set',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    const expected = '<div><p lang="en">This is in English' +
        '<a href="http://www.google.com/">This link has no language set</a>' +
        '</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent heading', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++heading htmlTag='h1' id=2
    // ++++staticText name='This is an h1.' id=3
    // ++heading htmlTag='h2' id=4
    // ++++staticText name='This is an h2.' id=5
    // ++heading htmlTag='h3' id=6
    // ++++staticText name='This is an h3.' id=7
    // ++heading htmlTag='h4' id=8
    // ++++staticText name='This is an h4.' id=9
    // ++heading htmlTag='h5' id=10
    // ++++staticText name='This is an h5.' id=11
    // ++heading htmlTag='h6' id=12
    // ++++staticText name='This is an h6.' id=13
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 4, 6, 8, 10, 12],
        },
        {
          id: 2,
          role: 'heading',
          htmlTag: 'h1',
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
          htmlTag: 'h2',
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
          htmlTag: 'h3',
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
          htmlTag: 'h4',
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
          htmlTag: 'h5',
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
          htmlTag: 'h6',
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
    const expected = '<div><h1>This is an h1.</h1><h2>This is an h2.</h2>' +
        '<h3>This is an h3.</h3><h4>This is an h4.</h4>' +
        '<h5>This is an h5.</h5><h6>This is an h6.</h6></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent link', () => {
    // Fake chrome.readAnything methods for the following AXTree
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
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><a href="http://www.google.com">This is a link.' +
        '</a><a href="http://www.youtube.com">This is another link.</a></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent link badInput', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++link htmlTag='a' id=2
    // ++++staticText name='This link does not have a url.' id=3
    // ++image htmlTag='img' url='http://www.mycat.com' id=4
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
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This link does not have a url.',
        },
        {
          id: 4,
          role: 'image',
          htmlTag: 'img',
          url: 'http://www.mycat.com',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><a>This link does not have a url.</a><img></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent staticText', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++staticText name='This is some text.' id=2
    // ++staticText name='This is some more text.' id=3
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
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
    const expected = '<div>This is some text.This is some more text.</div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent staticText badInput', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++staticText name='' id=2
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2],
        },
        {
          id: 2,
          role: 'staticText',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    const expected = '<div></div>';
    assertContainerInnerHTML(expected);
  });

  // The container clears its old content when it receives new content.
  test('updateContent clearContainer', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++staticText name='First set of content.' id=2
    const axTree1 = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
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
    const expected1 = '<div>First set of content.</div>';
    assertContainerInnerHTML(expected1);

    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++staticText name='Second set of content.' id=2
    const axTree2 = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
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
    const expected2 = '<div>Second set of content.</div>';
    assertContainerInnerHTML(expected2);
  });

  test('updateContent selection', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='Hello' id=3
    // ++paragraph htmlTag='p' id=4
    // ++++staticText name='World' id=5
    // ++paragraph htmlTag='p' id=6
    // ++++staticText name='Friend' id=7
    // ++++staticText name='!' id=8
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 4, 6],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'Hello',
        },
        {
          id: 4,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'World',
        },
        {
          id: 6,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [7, 8],
        },
        {
          id: 7,
          role: 'staticText',
          name: 'Friend',
        },
        {
          id: 8,
          role: 'staticText',
          name: '!',
        },
      ],
      selection: {
        anchor_object_id: 3,
        focus_object_id: 7,
        anchor_offset: 1,
        focus_offset: 2,
        is_backward: false,
      },
    };
    chrome.readAnything.setContentForTesting(axTree, []);
    // The expected string contains the selected text only inside of the node
    // that is common to the entire selection, which is the root node in this
    // example. Since the root node's html tag is '#document' which isn't valid,
    // we replace it with a div.
    const expected = '<div><p>ello</p><p>World</p><p>Fr</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent selection backwards', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='Hello' id=3
    // ++paragraph htmlTag='p' id=4
    // ++++staticText name='World' id=5
    // ++paragraph htmlTag='p' id=6
    // ++++staticText name='Friend' id=7
    // ++++staticText name='!' id=8
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 4, 6],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'Hello',
        },
        {
          id: 4,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'World',
        },
        {
          id: 6,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [7, 8],
        },
        {
          id: 7,
          role: 'staticText',
          name: 'Friend',
        },
        {
          id: 8,
          role: 'staticText',
          name: '!',
        },
      ],
      selection: {
        anchor_object_id: 7,
        focus_object_id: 3,
        anchor_offset: 2,
        focus_offset: 1,
        is_backward: true,
      },
    };
    chrome.readAnything.setContentForTesting(axTree, []);
    // The expected string contains the selected text only inside of the node
    // that is common to the entire selection, which is the root node in this
    // example. Since the root node's html tag is '#document' which isn't valid,
    // we replace it with a div.
    const expected = '<div><p>ello</p><p>World</p><p>Fr</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent setSelectedText', async () => {
    // root htmlTag='#document' id=1
    // ++staticText name='Hello' id=2
    // ++staticText name='World' id=3
    // ++staticText name='Friend' id=4
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 3, 4],
        },
        {
          id: 2,
          role: 'staticText',
          name: 'Hello',
        },
        {
          id: 3,
          role: 'staticText',
          name: 'World',
        },
        {
          id: 4,
          role: 'staticText',
          name: 'Friend',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [1]);
    const expected = '<div>HelloWorldFriend</div>';
    assertContainerInnerHTML(expected);

    // Create a selection of "elloWorldFr". The anchor node has id 2 and the
    // focus node has id 4.
    const outerDiv =
        readAnythingApp.shadowRoot!.getElementById(
                                       'container')!.firstElementChild;
    const range = new Range();
    range.setStart(outerDiv!.firstChild!, 1);
    range.setEnd(outerDiv!.lastChild!, 2);
    const selection = readAnythingApp.shadowRoot!.getSelection();
    selection!.removeAllRanges();
    selection!.addRange(range);

    // When the selection is set, readAnythingApp listens for the selection
    // change event and calls chrome.readAnything.onSelectionChange. This test
    // overrides that method and fires a custom event
    // 'on-selection-change-for-test' with the parameters to onSelectionChange
    // stored in details. Here, we check the values of the parameters of
    // onSelectionChange.
    const onSelectionChangeEvent = await eventToPromise(
        'on-selection-change-for-test', readAnythingApp.shadowRoot!);
    assertEquals(onSelectionChangeEvent.detail.anchorNodeId, 2);
    assertEquals(onSelectionChangeEvent.detail.anchorOffset, 1);
    assertEquals(onSelectionChangeEvent.detail.focusNodeId, 4);
    assertEquals(onSelectionChangeEvent.detail.focusOffset, 2);
  });

  test('updateContent textDirection', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2 direction='ltr'
    // ++++staticText name='This is left to right writing' id=3
    // ++paragraph htmlTag='p' id=4 direction='rtl'
    // ++++staticText name='This is right to left writing' id=4
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
          role: 'paragraph',
          htmlTag: 'p',
          direction: 1,
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is left to right writing',
        },
        {
          id: 4,
          role: 'paragraph',
          htmlTag: 'p',
          direction: 2,
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This is right to left writing',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><p dir="ltr">This is left to right writing</p>' +
        '<p dir="rtl">This is right to left writing</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent textDirection parentNodeDiffDir', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2 direction='ltr'
    // ++++staticText name='This is ltr' id=3
    // ++++link htmlTag='a' url='http://www.google.com/' id=4 direction='rtl'
    // ++++++staticText name='This link is rtl' id=5
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          direction: 1,
          childIds: [3, 4],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This is ltr',
        },
        {
          id: 4,
          role: 'link',
          htmlTag: 'a',
          direction: 2,
          url: 'http://www.google.com/',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This link is rtl',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    const expected = '<div><p dir="ltr">This is ltr' +
        '<a dir="rtl" href="http://www.google.com/">' +
        'This link is rtl</a></p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent textDirection verticalDir', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2 direction='ttb'
    // ++++staticText name='This should be auto' id=3
    // ++paragraph htmlTag='p' id=4 direction='btt'
    // ++++staticText name='This should be also be auto' id=4
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
          role: 'paragraph',
          htmlTag: 'p',
          direction: 3,
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'This should be auto',
        },
        {
          id: 4,
          role: 'paragraph',
          htmlTag: 'p',
          direction: 4,
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'This should be also be auto',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><p dir="auto">This should be auto</p>' +
        '<p dir="auto">This should be also be auto</p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent textStyle overline', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='This should be overlined.' textStyle='overline' id=3
    // ++++staticText name='Regular text.' id=4
    // ++++staticText name='This is overlined and bolded.' textStyle='overline
    // underline'id=5
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [3, 4, 5],
        },
        {
          id: 3,
          role: 'staticText',
          textStyle: 'overline',
          name: 'This should be overlined.',
        },
        {
          id: 4,
          role: 'staticText',
          name: 'Regular text.',
        },
        {
          id: 5,
          role: 'staticText',
          textStyle: 'overline underline',
          name: 'This is overlined and bolded.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2]);
    const expected = '<div><p><span style="text-decoration: overline;">This ' +
        'should be overlined.</span>Regular text.<b style="text-decoration: ' +
        'overline;">This is overlined and bolded.</b></p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent textStyle bold', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='Regular text.' id=3
    // ++++staticText name='This should be bolded.' textStyle='underline' id=4
    // ++paragraph htmlTag='p' id=5
    // ++++staticText name='Bolded text.' textStyle='italic' id=6
    // ++++staticText name='Bolded text.' textStyle='bold' id=7
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 5],
        },
        {
          id: 2,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [3, 4],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'Regular text.',
        },
        {
          id: 4,
          role: 'staticText',
          textStyle: 'underline',
          name: 'This should be bolded.',
        },
        {
          id: 5,
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [6, 7],
        },
        {
          id: 6,
          role: 'staticText',
          textStyle: 'italic',
          name: 'Bolded text.',
        },
        {
          id: 7,
          role: 'staticText',
          textStyle: 'bold',
          name: 'Bolded text.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 5]);
    const expected = '<div><p>Regular text.<b>This should be bolded.</b></p>' +
        '<p><b>Bolded text.</b><b>Bolded text.</b></p></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent noContentNodes', () => {
    // Fake chrome.readAnything methods for the following AXTree
    // root htmlTag='#document' id=1
    // ++staticText name='This is some text.' id=2
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2],
        },
        {
          id: 2,
          role: 'staticText',
          name: 'This is some text.',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, []);
    const expected = '<div></div>';
    assertContainerInnerHTML(expected);
  });

  test('updateContent interactiveElement', () => {
    // root htmlTag='#document' id=1
    // ++paragraph htmlTag='p' id=2
    // ++++staticText name='hello world' id=3
    // ++button htmlTag='button' id=4
    // ++++staticText name='button text' id=5
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
          role: 'paragraph',
          htmlTag: 'p',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'hello world',
        },
        {
          id: 4,
          role: 'button',
          htmlTag: 'button',
          childIds: [5],
        },
        {
          id: 5,
          role: 'staticText',
          name: 'button text',
        },
      ],
    };
    chrome.readAnything.setContentForTesting(axTree, [2, 4]);
    const expected = '<div><p>hello world</p></div>';
    assertContainerInnerHTML(expected);
  });
});

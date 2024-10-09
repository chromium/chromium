// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';

suite('UpdateContentIntegration', () => {
  let app: AppElement;

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

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
  });

  test('overline text style', () => {
    const nodes = [
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
    ];
    const expected = '<div><p><span style="text-decoration: overline;">This ' +
        'should be overlined.</span>Regular text.<b style="text-decoration: ' +
        'overline;">This is overlined and bolded.</b></p></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });

  test('bold text style', () => {
    const nodes = [
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
    ];
    const expected = '<div><p>Regular text.<b>This should be bolded.</b></p>' +
        '<p><b>Bolded text.</b><b>Bolded text.</b></p></div>';

    setTree([2, 5], nodes);

    assertHtml(expected);
  });

  test('horizontal text direction', () => {
    const nodes = [
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
    ];
    const expected = '<div><p dir="ltr">This is left to right writing</p>' +
        '<p dir="rtl">This is right to left writing</p></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('horizontal text direction with different parent', () => {
    const nodes = [
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
    ];
    const expected = '<div><p dir="ltr">This is ltr' +
        '<a dir="rtl" href="http://www.google.com/">' +
        'This link is rtl</a></p></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });

  test('vertical text direction', () => {
    const nodes = [
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
    ];
    const expected = '<div><p dir="auto">This should be auto</p>' +
        '<p dir="auto">This should be also be auto</p></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('normal text', () => {
    const nodes = [
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
    ];
    const expected = '<div>This is some text.This is some more text.</div>';

    setTree([2, 3], nodes);

    assertHtml(expected);
  });

  test('bad input', () => {
    const nodes = [
      {
        id: 2,
        role: 'staticText',
      },
    ];
    const expected = '';

    setTree([2], nodes);

    assertHtml(expected);
  });

  test('paragraph', () => {
    const nodes = [
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
    ];
    const expected = '<div><p>This is a paragraph</p>' +
        '<p>This is a second paragraph</p></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('no content nodes', () => {
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
    const expected = '';

    chrome.readingMode.setContentForTesting(axTree, []);

    assertHtml(expected);
  });

  test('link', () => {
    const nodes = [
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
    ];
    const expected = '<div><a href="http://www.google.com">This is a link.' +
        '</a><a href="http://www.youtube.com">This is another link.</a></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('bad link input', () => {
    const nodes = [
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
        role: 'link',
        htmlTag: 'a',
      },
    ];
    const expected = '<div><a>This link does not have a url.</a><a></a></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('language set by parent', () => {
    const nodes = [
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
    ];
    const expected = '<div><p lang="en">This is in English' +
        '<a href="http://www.google.com/">This link has no language set</a>' +
        '</p></div>';

    setTree([2], nodes);

    assertHtml(expected);
  });

  test('child and parent language are different', () => {
    const nodes = [
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
    ];
    const expected = '<div><p lang="en">This is in English</p>' +
        '<p lang="es">Esto es en español' +
        '<a href="http://www.google.cn/" lang="zh">' +
        'This is a link in Chinese</a></p></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('interactive element', () => {
    const nodes = [
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
    ];
    const expected = '<div><p>hello world</p></div>';

    setTree([2, 4], nodes);

    assertHtml(expected);
  });

  test('headings with other content', () => {
    const nodes = [
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
      {
        id: 14,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [15],
      },
      {
        id: 15,
        role: 'staticText',
        name: 'This is a paragraph.',
      },
    ];
    const expected = '<div><h1>This is an h1.</h1><h2>This is an h2.</h2>' +
        '<h3>This is an h3.</h3><h4>This is an h4.</h4><h5>This is an h5.' +
        '</h5><h6>This is an h6.</h6><p>This is a paragraph.</p></div>';

    setTree([2, 4, 6, 8, 10, 12, 14], nodes);

    assertHtml(expected);
  });

  test('only headings', () => {
    const nodes = [
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
    ];
    const expected = '';

    // RM should not display JUST heading nodes.
    setTree([2, 4, 6, 8, 10, 12], nodes);

    assertHtml(expected);
  });
});

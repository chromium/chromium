// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {renderMarkdown} from 'chrome-untrusted://glic/markdown.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('Markdown', () => {
  function assertRenders(markdown: string, html: string) {
    const div = document.createElement('div');
    div.append(renderMarkdown(markdown));
    const expected = html.split('\n').map(line => line.trim()).join('');
    assertEquals(expected, div.innerHTML);
  }

  test('Paragraphs', () => {
    const markdown = `
      a
      b

      c`;
    const html = `
      <p>a</p>
      <p>b</p>
      <p>c</p>`;
    assertRenders(markdown, html);
  });

  test('BoldAndItalic', () => {
    const markdown = `
      **a *b* c** *d* ***e***`;
    const html = `
      <p>
        <strong>a <em>b</em> c</strong> <em>d</em> <strong><em>e</em></strong>
      </p>`;
    assertRenders(markdown, html);
  });

  test('LoneMarkersAreText', () => {
    const markdown = `
      2 * 3 = 6
      2 ** 3 = 8 and *a*`;
    const html = `
      <p>2 * 3 = 6</p>
      <p>2 ** 3 = 8 and <em>a</em></p>`;
    assertRenders(markdown, html);
  });

  test('Lists', () => {
    const markdown = `
      - a

      * b
      c
      - d`;
    const html = `
      <ul>
        <li>a</li>
        <li>b</li>
      </ul>
      <p>c</p>
      <ul>
        <li>d</li>
      </ul>`;
    assertRenders(markdown, html);
  });

  test('OrderedLists', () => {
    const markdown = `
      1. a
      2. b
         - c
      3. d
      10. e
      1.5 kg`;
    const html = `
      <ol start="1">
        <li>a</li>
        <li>b</li>
      </ol>
      <ul>
        <li>c</li>
      </ul>
      <ol start="3">
        <li>d</li>
      </ol>
      <p>10. e</p>
      <p>1.5 kg</p>`;
    assertRenders(markdown, html);
  });

  test('Links', () => {
    const markdown = `
      See **[a](https://b.test)**, [1] and [c](d).`;
    const html = `
      <p>
        See <strong><a href="#">a</a></strong>, [1] and <a href="#">c</a>.
      </p>`;
    assertRenders(markdown, html);
  });

  test('LinksWinOverBoldAndItalic', () => {
    const markdown = `
      [**a**](b)
      **c [d** e](f)
      *[g*](h)
      [i](https://j.test/*k*)`;
    const html = `
      <p><a href="#"><strong>a</strong></a></p>
      <p>**c <a href="#">d** e</a></p>
      <p>*<a href="#">g*</a></p>
      <p><a href="#">i</a></p>`;
    assertRenders(markdown, html);
  });

  test('OtherMarkdownIsText', () => {
    const markdown = `
      # a
      1) b
      \`c\` <i>`;
    const html = `
      <p># a</p>
      <p>1) b</p>
      <p>\`c\` &lt;i&gt;</p>`;
    assertRenders(markdown, html);
  });
});

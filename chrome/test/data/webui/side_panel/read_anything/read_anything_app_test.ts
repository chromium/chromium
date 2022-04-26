// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/read_anything/app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ReadAnythingElement} from 'chrome://read-later.top-chrome/read_anything/app.js';
import {HeadingElement} from 'chrome://read-later.top-chrome/read_anything/heading_element.js';
import {ContentNode, ContentType} from 'chrome://read-later.top-chrome/read_anything/read_anything.mojom-webui.js';
import {PageRemote} from 'chrome://read-later.top-chrome/read_anything/read_anything.mojom-webui.js';
import {ReadAnythingApiProxyImpl} from 'chrome://read-later.top-chrome/read_anything/read_anything_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestReadAnythingApiProxy} from './test_read_anything_api_proxy.js';

suite('ReadAnythingAppTest', () => {
  let readAnythingApp: ReadAnythingElement;
  let testProxy: TestReadAnythingApiProxy;
  let callbackRouter: PageRemote;

  setup(async () => {
    testProxy = new TestReadAnythingApiProxy();
    ReadAnythingApiProxyImpl.setInstance(testProxy);
    callbackRouter = testProxy.getCallbackRouterRemote();

    document.body.innerHTML = '';
    readAnythingApp = document.createElement('read-anything-app');
    document.body.appendChild(readAnythingApp);
    await flushTasks();
  });

  function assertFontName(fontFamily: string) {
    const container = readAnythingApp.shadowRoot!.getElementById('container');
    assertEquals(fontFamily, getComputedStyle(container!).fontFamily);
  }

  function assertContentNode(contentNode: ContentNode, element: Element) {
    let expectedTagName;
    switch (contentNode.type) {
      case ContentType.kHeading:
        switch (contentNode.headingLevel) {
          case 1:
            expectedTagName = 'H1';
            break;
          case 3:
            expectedTagName = 'H3';
            break;
          case 4:
            expectedTagName = 'H4';
            break;
          case 5:
            expectedTagName = 'H5';
            break;
          case 6:
            expectedTagName = 'H6';
            break;
          case 2:
          default:  // Heading levels default to H2; see heading_element.ts.
            expectedTagName = 'H2';
            break;
        }
        break;
      case ContentType.kParagraph:
        expectedTagName = 'P';
        break;
    }
    assertEquals(expectedTagName, element.tagName);
    assertEquals(contentNode.text, element.textContent);
  }

  test('onFontNameChange', async () => {
    callbackRouter.onFontNameChange('Standard font');
    await flushTasks();
    assertFontName('Roboto, Arial, sans-serif');

    callbackRouter.onFontNameChange('Sans-serif');
    await flushTasks();
    assertFontName('Roboto, Tahoma, sans-serif');

    callbackRouter.onFontNameChange('Serif');
    await flushTasks();
    assertFontName('Didot, Georgia, serif');

    callbackRouter.onFontNameChange('Arial');
    await flushTasks();
    assertFontName('Arial, Verdona, sans-serif');

    callbackRouter.onFontNameChange('Open Sans');
    await flushTasks();
    assertFontName('"Open Sans", Courier, sans-serif');

    callbackRouter.onFontNameChange('Calibri');
    await flushTasks();
    assertFontName('Calibri, "Times New Roman", serif');
  });

  test('showContent paragraph', async () => {
    const contentNodes: ContentNode[] = [
      {
        type: ContentType.kParagraph,
        text: 'This is a paragraph.',
        headingLevel: 0,
      },
      {
        type: ContentType.kParagraph,
        text: 'This is a second paragraph.',
        headingLevel: 0,
      },
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();

    const paragraphs: HTMLElement[] =
        Array.from(readAnythingApp.shadowRoot!.querySelectorAll('p'));
    assertEquals(contentNodes.length, paragraphs.length);
    for (let i = 0; i < paragraphs.length; i++) {
      assertContentNode(contentNodes[i]!, paragraphs[i]!);
    }
  });

  test('showContent heading', async () => {
    const contentNodes: ContentNode[] = [
      {
        type: ContentType.kHeading,
        text: 'This is an h1.',
        headingLevel: 1,
      },
      {
        type: ContentType.kHeading,
        text: 'This is an h2.',
        headingLevel: 2,
      },
      {
        type: ContentType.kHeading,
        text: 'This is an h3.',
        headingLevel: 3,
      },
      {
        type: ContentType.kHeading,
        text: 'This is an h4.',
        headingLevel: 5,
      },
      {
        type: ContentType.kHeading,
        text: 'This is an h5.',
        headingLevel: 5,
      },
      {
        type: ContentType.kHeading,
        text: 'This is an h6.',
        headingLevel: 6,
      },
      {
        type: ContentType.kHeading,
        text: 'This is a heading with an improper heading level.',
        headingLevel: 0,
      },
      {
        type: ContentType.kHeading,
        text: 'This is another heading with an improper heading level.',
        headingLevel: 7,
      },
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();

    const headings: HeadingElement[] = Array.from(
        readAnythingApp.shadowRoot!.querySelectorAll('read-anything-heading'));
    assertEquals(contentNodes.length, headings.length);
    for (let i = 0; i < headings.length; i++) {
      const heading =
          headings[i]!.shadowRoot!.querySelector('h1, h2, h3, h4, h5, h6');
      assertContentNode(contentNodes[i]!, heading!);
    }
  });
});

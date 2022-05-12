// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/read_anything/app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ReadAnythingElement} from 'chrome://read-later.top-chrome/read_anything/app.js';
import {ContentNode, ContentType, PageRemote} from 'chrome://read-later.top-chrome/read_anything/read_anything.mojom-webui.js';
import {ReadAnythingApiProxyImpl} from 'chrome://read-later.top-chrome/read_anything/read_anything_api_proxy.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertNotReached} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestReadAnythingApiProxy} from './test_read_anything_api_proxy.js';

class ContentNodeBuilder {
  children: ContentNode[];
  headingLevel: number;
  text: string;
  type: ContentType;
  url: Url;

  constructor(type: ContentType) {
    this.children = [];
    this.headingLevel = 0;
    this.text = '';
    this.type = type;
    this.url = {url: ''};
  }

  setChildren(children: ContentNode[]) {
    this.children = children;
    return this;
  }

  setHeadingLevel(headingLevel: number) {
    this.headingLevel = headingLevel;
    return this;
  }

  setText(text: string) {
    this.text = text;
    return this;
  }

  setUrl(url: string) {
    this.url = {url: url};
    return this;
  }

  build() {
    const contentNode: ContentNode = {
      children: this.children,
      headingLevel: this.headingLevel,
      text: this.text,
      type: this.type,
      url: this.url,
    };
    return contentNode;
  }
}

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

  function assertHeading(contentNode: ContentNode, node: Node) {
    assertEquals(Node.ELEMENT_NODE, node.nodeType);
    const tagName: string = 'H' + contentNode.headingLevel;
    const element: HTMLElement = node as HTMLElement;
    assertEquals(tagName, element.tagName);
  }

  function assertLink(contentNode: ContentNode, node: Node) {
    assertEquals(Node.ELEMENT_NODE, node.nodeType);
    const element: HTMLElement = node as HTMLElement;
    assertEquals('A', element.tagName);
    assertEquals(contentNode.url.url, element.getAttribute('href'));
  }

  function assertParagraph(node: Node) {
    assertEquals(Node.ELEMENT_NODE, node.nodeType);
    const element: HTMLElement = node as HTMLElement;
    assertEquals('P', element.tagName);
  }

  function assertStaticText(contentNode: ContentNode, node: Node) {
    assertEquals(Node.TEXT_NODE, node.nodeType);
    assertEquals(contentNode.text, node.textContent);
  }

  function assertContentNode(contentNode: ContentNode, node: Node) {
    switch (contentNode.type) {
      case ContentType.kHeading:
        assertHeading(contentNode, node);
        break;
      case ContentType.kLink:
        assertLink(contentNode, node);
        break;
      case ContentType.kParagraph:
        assertParagraph(node);
        break;
      case ContentType.kStaticText:
        assertStaticText(contentNode, node);
        break;
      default:
        assertNotReached('ContentNode must be a defined type.');
        break;
    }

    const childNodes: Node[] = Array.from(node.childNodes);
    assertEquals(contentNode.children.length, childNodes.length);
    for (let i = 0; i < contentNode.children.length; i++) {
      assertContentNode(contentNode.children[i]!, childNodes[i]!);
    }
  }

  function assertContentNodes(contentNodes: ContentNode[]) {
    const nodes: Node[] = Array.from(
        readAnythingApp.shadowRoot!.getElementById('container')!.childNodes);
    assertEquals(contentNodes.length, nodes.length);
    for (let i = 0; i < contentNodes.length; i++) {
      assertContentNode(contentNodes[i]!, nodes[i]!);
    }
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
      new ContentNodeBuilder(ContentType.kParagraph)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is a paragraph.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kParagraph)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is a second paragraph.')
                            .build()])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    assertContentNodes(contentNodes);
  });

  test('showContent heading', async () => {
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(1)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h1.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(2)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h2.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(3)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h3.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(4)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h4.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(5)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h5.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(6)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is an h6.')
                            .build()])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    assertContentNodes(contentNodes);
  });

  test('showContent heading badInput', async () => {
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(0)
          .setChildren(
              [new ContentNodeBuilder(ContentType.kStaticText)
                   .setText('This is a heading with an improper heading level.')
                   .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kHeading)
          .setHeadingLevel(7)
          .setChildren([
            new ContentNodeBuilder(ContentType.kStaticText)
                .setText(
                    'This is another heading with an improper heading level.')
                .build()
          ])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();

    // Heading levels default to 2.
    const expected: ContentNode[] = contentNodes;
    expected[0]!.headingLevel = 2;
    expected[1]!.headingLevel = 2;
    assertContentNodes(expected);
  });

  test('showContent link', async () => {
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kLink)
          .setUrl('http://www.google.com')
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is a link.')
                            .build()])
          .build(),
      new ContentNodeBuilder(ContentType.kLink)
          .setUrl('http://www.youtube.com')
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This is another link.')
                            .build()])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    assertContentNodes(contentNodes);
  });

  test('showContent link badInput', async () => {
    // Links must have a url.
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kLink)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This link does not have a url.')
                            .build()])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    const expected: ContentNode[] = [];
    assertContentNodes(expected);
  });

  test('showContent staticText', async () => {
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kStaticText)
          .setText('This is some text.')
          .build(),
      new ContentNodeBuilder(ContentType.kStaticText)
          .setText('This is some more text.')
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    assertContentNodes(contentNodes);
  });

  test('showContent staticText badInput', async () => {
    // Static text nodes must have text and must not have children.
    const contentNodes: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kStaticText).build(),
      new ContentNodeBuilder(ContentType.kStaticText).setText('').build(),
      new ContentNodeBuilder(ContentType.kStaticText)
          .setChildren([new ContentNodeBuilder(ContentType.kStaticText)
                            .setText('This text has children.')
                            .build()])
          .build(),
    ];

    callbackRouter.showContent(contentNodes);
    await flushTasks();
    const expected: ContentNode[] = [];
    assertContentNodes(expected);
  });

  test('showContent clearContainer', async () => {
    const contentNodes1: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kStaticText)
          .setText('First set of content.')
          .build(),
    ];

    callbackRouter.showContent(contentNodes1);
    await flushTasks();
    assertContentNodes(contentNodes1);

    const contentNodes2: ContentNode[] = [
      new ContentNodeBuilder(ContentType.kStaticText)
          .setText('Second set of content.')
          .build(),
    ];

    callbackRouter.showContent(contentNodes2);
    await flushTasks();
    assertContentNodes(contentNodes2);
  });
});

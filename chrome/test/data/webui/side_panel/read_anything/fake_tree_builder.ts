// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import type {FakeReadingMode} from './fake_reading_mode.js';

// A tree to be used in tests alongside a FakeReadingMode.
export class FakeTree {
  // Flat map of all nodes in the tree from id to the node object
  nodes: Map<number, FakeTreeNode>;
  readingMode: FakeReadingMode;

  constructor(nodes: Map<number, FakeTreeNode>, readingMode: FakeReadingMode) {
    this.nodes = nodes;
    this.readingMode = readingMode;
  }

  // Sets the current reading highlight on this fake tree to be the entire node
  highlightNode(id: number) {
    assertTrue(
        this.nodes.has(id),
        'You\'re trying to highlight a node that is not in this tree!');
    this.setReadingHighlight(
        id, 0, id, this.nodes.get(id!)!.textContent!.length);
  }

  // Sets the current reading highlight on this fake tree
  setReadingHighlight(
      fromId: number, fromOffset: number, toId: number, toOffset: number) {
    assertTrue(
        this.nodes.has(fromId) && this.nodes.has(toId),
        'You\'re trying to highlight a node that is not in this tree!');
    this.readingMode.getCurrentTextStartIndex = id => {
      switch (id) {
        case fromId:
          return fromOffset!;
        case toId:
          return 0;
        default:
          return -1;
      }
    };

    this.readingMode.getCurrentTextEndIndex = id => {
      switch (id) {
        case toId:
          return toOffset!;
        case fromId:
          return this.nodes.get(fromId!)!.textContent!.length;
        default:
          return -1;
      }
    };
  }

  // Sets the main panel selection for this fake tree
  setSelection(
      fromId: number, fromOffset: number, toId: number, toOffset: number) {
    assertTrue(
        this.nodes.has(fromId) && this.nodes.has(toId),
        'You\'re trying to select a node that is not in this tree!');
    this.readingMode.startNodeId = fromId;
    this.readingMode.startOffset = fromOffset;
    this.readingMode.endNodeId = toId;
    this.readingMode.endOffset = toOffset;
  }
}

// Creates a fake tree for unit testing without needing to go through the whole
// C++ pipeline to build a tree.
export class FakeTreeBuilder {
  // The root of the tree
  rootNode?: FakeTreeNode;
  // Flat map of all nodes in the tree from id to the node object
  nodes: Map<number, FakeTreeNode> = new Map();

  // Finalizes the tree by updating the given fake reading mode to return the
  // correct values
  build(readingMode: FakeReadingMode): FakeTree {
    assertTrue(
        !!this.rootNode, 'You need to set the root node before building!');
    readingMode.rootId = this.rootNode.id;
    readingMode.getHtmlTag = id => {
      return this.nodes.get(id)!.htmlTag!;
    };
    readingMode.getTextContent = id => {
      return this.nodes.get(id)!.textContent!;
    };
    readingMode.getChildren = id => {
      return this.nodes.get(id)!.children;
    };

    return new FakeTree(this.nodes, readingMode);
  }

  // Creates the root node
  root(id: number): FakeTreeBuilder {
    this.rootNode = new FakeTreeNode(id, '#document');
    this.nodes.set(id, this.rootNode);
    return this;
  }

  // Adds a child node that represents an html tag
  addTag(id: number, parentId: number, htmlTag: string): FakeTreeBuilder {
    const child = new FakeTreeNode(id, htmlTag);
    const parent = this.nodes.get(parentId);
    assertTrue(!!parent, 'You need to add the parent node for this first!');
    parent.addChild(id);
    this.nodes.set(id, child);
    return this;
  }

  // Adds a child node that represents a static text
  addText(id: number, parentId: number, textContent: string): FakeTreeBuilder {
    const child = new FakeTreeNode(id, '', textContent);
    const parent = this.nodes.get(parentId);
    assertTrue(!!parent, 'You need to add the parent node for this first!');
    parent.addChild(id);
    this.nodes.set(id, child);
    return this;
  }
}

// A single node in a FakeTree.
class FakeTreeNode {
  id: number;
  htmlTag: string;
  textContent: string;
  children: number[] = [];

  constructor(id: number, htmlTag?: string, textContent?: string) {
    this.id = id;
    this.htmlTag = htmlTag ? htmlTag : '';
    this.textContent = textContent ? textContent : '';
  }

  addChild(nodeId: number): void {
    this.children.push(nodeId);
  }
}

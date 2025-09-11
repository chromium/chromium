// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, NodeStore, SelectionController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {SelectionWithIds} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('SelectionController', () => {
  let selectionController: SelectionController;
  let nodeStore: NodeStore;

  const parentIds = [100, 200, 300, 400];
  const textNodeIds = [3, 5, 7, 9];
  const texts = [
    'From the day we arrive on the planet',
    'And blinking, step into the sun',
    'Theres more to see than can ever be seen more to do than can ever be done',
    'In the circle of life',
  ];
  let textNodes: Text[];

  interface TextNode {
    node: Text;
    id: number;
    text: string;
  }

  function getNodeAt(nodeIndex: number): TextNode {
    const node = textNodes[nodeIndex];
    const id = textNodeIds[nodeIndex];
    const text = texts[nodeIndex];
    assertTrue(!!node);
    assertTrue(!!id);
    assertTrue(!!text);
    return {node, id, text};
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    selectionController = new SelectionController();
    SelectionController.setInstance(selectionController);
  });

  suite('getSelectionAdjustedForHighlights', () => {
    function highlightEverything() {
      for (let i = 0; i < textNodes.length; i++) {
        highlightAtOffset(i, 0);
      }
    }

    function highlightAtOffset(nodeIndex: number, offset: number) {
      const child = textNodes[nodeIndex];
      const parentId = parentIds[nodeIndex];
      assertTrue(!!child);
      assertTrue(!!parentId);
      const parent = document.createElement('span');
      assertTrue(!!child.textContent);
      parent.appendChild(document.createTextNode(child.textContent));
      child.replaceWith(parent);
      nodeStore.setDomNode(parent, parentId);
      nodeStore.setAncestor(child, parent, offset);
    }

    function selectNodes(
        anchor: TextNode, anchorOffset: number, focus: TextNode,
        focusOffset: number): SelectionWithIds {
      return selectionController.getSelectionAdjustedForHighlights(
          anchor.node, anchorOffset, focus.node, focusOffset);
    }

    setup(() => {
      textNodes = texts.map(str => document.createTextNode(str));
      assertEquals(textNodeIds.length, textNodes.length);
    });

    test('gets node when node exists as is', () => {
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      const node = getNodeAt(1);
      nodeStore.setDomNode(node.node, node.id);

      const selection =
          selectNodes(node, expectedAnchorOffset, node, expectedFocusOffset);

      assertEquals(node.id, selection.anchorNodeId);
      assertEquals(node.id, selection.focusNodeId);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('gets undefined when node does not exist and has no ancestor', () => {
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      const node = getNodeAt(1);

      const selection =
          selectNodes(node, expectedAnchorOffset, node, expectedFocusOffset);

      assertFalse(!!selection.anchorNodeId);
      assertFalse(!!selection.focusNodeId);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('one node selected with ancestor and no ancestor offset', () => {
      highlightEverything();
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 7;
      const node = getNodeAt(0);

      const selection =
          selectNodes(node, expectedAnchorOffset, node, expectedFocusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[0], selection.focusNodeId);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('multiple nodes selected with ancestor and no ancestor offset', () => {
      highlightEverything();
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);

      const selection =
          selectNodes(node1, expectedAnchorOffset, node2, expectedFocusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[1], selection.focusNodeId);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('one node selected with anchor offset in ancestor', () => {
      const highlightStart = 15;
      highlightAtOffset(0, highlightStart);
      const node = getNodeAt(0);
      const anchorOffset = 5;
      const focusOffset = node.text.length;

      const selection = selectNodes(node, anchorOffset, node, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[0], selection.focusNodeId);
      assertEquals(highlightStart + anchorOffset, selection.anchorOffset);
      assertEquals(highlightStart + focusOffset, selection.focusOffset);
    });

    test('multiple nodes selected with anchor offset in ancestor', () => {
      const highlightStart = 15;
      highlightAtOffset(0, highlightStart);
      highlightAtOffset(1, 0);
      const anchorNode = getNodeAt(0);
      const focusNode = getNodeAt(1);
      const anchorOffset = 5;
      const focusOffset = 10;

      const selection =
          selectNodes(anchorNode, anchorOffset, focusNode, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[1], selection.focusNodeId);
      assertEquals(highlightStart + anchorOffset, selection.anchorOffset);
      assertEquals(focusOffset, selection.focusOffset);
    });

    test('multiple nodes selected with focus offset in ancestor', () => {
      const highlightStart = 15;
      highlightAtOffset(0, 0);
      highlightAtOffset(1, highlightStart);
      const anchorNode = getNodeAt(0);
      const focusNode = getNodeAt(1);
      const anchorOffset = 5;
      const focusOffset = 10;

      const selection =
          selectNodes(anchorNode, anchorOffset, focusNode, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[1], selection.focusNodeId);
      assertEquals(anchorOffset, selection.anchorOffset);
      assertEquals(highlightStart + focusOffset, selection.focusOffset);
    });

    test('multiple nodes selected with anchor ancestor only', () => {
      highlightAtOffset(0, 0);
      const anchorNode = getNodeAt(0);
      const focusNode = getNodeAt(1);
      nodeStore.setDomNode(focusNode.node, focusNode.id);
      const anchorOffset = 6;
      const focusOffset = 11;

      const selection =
          selectNodes(anchorNode, anchorOffset, focusNode, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(focusNode.id, selection.focusNodeId);
      assertEquals(anchorOffset, selection.anchorOffset);
      assertEquals(focusOffset, selection.focusOffset);
    });

    test('multiple nodes selected with focus ancestor only', () => {
      highlightAtOffset(1, 0);
      const anchorNode = getNodeAt(0);
      const focusNode = getNodeAt(1);
      nodeStore.setDomNode(anchorNode.node, anchorNode.id);
      const anchorOffset = 6;
      const focusOffset = 11;

      const selection =
          selectNodes(anchorNode, anchorOffset, focusNode, focusOffset);

      assertEquals(anchorNode.id, selection.anchorNodeId);
      assertEquals(parentIds[1], selection.focusNodeId);
      assertEquals(anchorOffset, selection.anchorOffset);
      assertEquals(focusOffset, selection.focusOffset);
    });

    test('selection with both anchor and focus offset in ancestors', () => {
      const anchorHighlightStart = 10;
      const focusHighlightStart = 20;
      highlightAtOffset(0, anchorHighlightStart);
      highlightAtOffset(1, focusHighlightStart);

      const anchorNode = getNodeAt(0);
      const focusNode = getNodeAt(1);
      const anchorOffset = 5;
      const focusOffset = 15;

      const selection =
          selectNodes(anchorNode, anchorOffset, focusNode, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[1], selection.focusNodeId);
      assertEquals(anchorHighlightStart + anchorOffset, selection.anchorOffset);
      assertEquals(focusHighlightStart + focusOffset, selection.focusOffset);
    });

    test('selection starts at offset 0 with an ancestor offset', () => {
      const highlightStart = 15;
      highlightAtOffset(0, highlightStart);
      const node = getNodeAt(0);
      const anchorOffset = 0;
      const focusOffset = 10;

      const selection = selectNodes(node, anchorOffset, node, focusOffset);

      assertEquals(parentIds[0], selection.anchorNodeId);
      assertEquals(parentIds[0], selection.focusNodeId);
      assertEquals(highlightStart + anchorOffset, selection.anchorOffset);
      assertEquals(highlightStart + focusOffset, selection.focusOffset);
    });
  });

  suite('updateSelection', () => {
    let parent: HTMLElement;
    const parentId = 111;
    let selection: Selection;

    function selectNodesInMainPanel(
        anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number) {
      chrome.readingMode.startNodeId = anchorId;
      chrome.readingMode.startOffset = anchorOffset;
      chrome.readingMode.endNodeId = focusId;
      chrome.readingMode.endOffset = focusOffset;
    }

    setup(() => {
      textNodes = texts.map(str => document.createTextNode(str));
      assertEquals(textNodeIds.length, textNodes.length);
      parent = document.createElement('p');
      textNodes.forEach(node => {
        parent.appendChild(node);
      });
      nodeStore.setDomNode(parent, parentId);
      document.body.appendChild(parent);
      const docSelection = document.getSelection();
      assertTrue(!!docSelection);
      selection = docSelection;
    });

    test('selects nodes directly if they are text nodes', () => {
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      const node = getNodeAt(0);
      nodeStore.setDomNode(node.node, node.id);

      selectNodesInMainPanel(
          node.id, expectedAnchorOffset, node.id, expectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node.node, selection.anchorNode);
      assertEquals(node.node, selection.focusNode);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('does nothing when node is undefined', () => {
      const node = getNodeAt(1);

      selectNodesInMainPanel(node.id, 2, node.id, 10);
      selectionController.updateSelection(selection);

      assertFalse(!!selection.anchorNode);
      assertFalse(!!selection.focusNode);
    });

    test('selection of first child via parent', () => {
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 7;
      const node = getNodeAt(0);

      selectNodesInMainPanel(
          parentId, expectedAnchorOffset, parentId, expectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node.text, selection.anchorNode?.textContent);
      assertEquals(node.text, selection.focusNode?.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('selection of first and second child via parent', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      const relativeAnchorOffset = 1;
      const relativeFocusOffset = 7;
      const selectedFocusOffset = node1.text.length + relativeFocusOffset;

      // The main panel selection is relative to the paragraph.
      selectNodesInMainPanel(
          parentId, relativeAnchorOffset, parentId, selectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node1.text, selection.anchorNode?.textContent);
      assertEquals(node2.text, selection.focusNode?.textContent);
      // The RM selection is relative to the node.
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of second child via parent', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      const relativeAnchorOffset = 1;
      const selectedAnchorOffset = node1.text.length + relativeAnchorOffset;
      const relativeFocusOffset = 7;
      const selectedFocusOffset = node1.text.length + relativeFocusOffset;

      // The main panel selection is relative to the paragraph.
      selectNodesInMainPanel(
          parentId, selectedAnchorOffset, parentId, selectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node2.text, selection.anchorNode?.textContent);
      assertEquals(node2.text, selection.focusNode?.textContent);
      // The RM selection is relative to the node.
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of second and third child via parent', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      const node3 = getNodeAt(2);
      const relativeAnchorOffset = 1;
      const selectedAnchorOffset = node1.text.length + relativeAnchorOffset;
      const relativeFocusOffset = 7;
      const selectedFocusOffset =
          node1.text.length + node2.text.length + relativeFocusOffset;

      // The main panel selection is relative to the paragraph.
      selectNodesInMainPanel(
          parentId, selectedAnchorOffset, parentId, selectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node2.text, selection.anchorNode?.textContent);
      assertEquals(node3.text, selection.focusNode?.textContent);
      // The RM selection is relative to the node.
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of first child via parent and second child itself', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      nodeStore.setDomNode(node2.node, node2.id);
      const relativeAnchorOffset = 1;
      const relativeFocusOffset = 7;

      selectNodesInMainPanel(
          parentId, relativeAnchorOffset, node2.id, relativeFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node1.text, selection.anchorNode?.textContent);
      assertEquals(node2.text, selection.focusNode?.textContent);
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of first child itself and second child via parent', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      nodeStore.setDomNode(node1.node, node1.id);
      const relativeAnchorOffset = 1;
      const relativeFocusOffset = 7;
      const selectedFocusOffset = node1.text.length + relativeFocusOffset;

      selectNodesInMainPanel(
          node1.id, relativeAnchorOffset, parentId, selectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node1.text, selection.anchorNode?.textContent);
      assertEquals(node2.text, selection.focusNode?.textContent);
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of second child via parent and third child itself', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      const node3 = getNodeAt(2);
      nodeStore.setDomNode(node3.node, node3.id);
      const relativeAnchorOffset = 1;
      const selectedAnchorOffset = node1.text.length + relativeAnchorOffset;
      const relativeFocusOffset = 7;

      selectNodesInMainPanel(
          parentId, selectedAnchorOffset, node3.id, relativeFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node2.text, selection.anchorNode?.textContent);
      assertEquals(node3.text, selection.focusNode?.textContent);
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });

    test('selection of second child itself and third child via parent', () => {
      const node1 = getNodeAt(0);
      const node2 = getNodeAt(1);
      const node3 = getNodeAt(2);
      nodeStore.setDomNode(node2.node, node2.id);
      const relativeAnchorOffset = 1;
      const relativeFocusOffset = 7;
      const selectedFocusOffset =
          node1.text.length + node2.text.length + relativeFocusOffset;

      selectNodesInMainPanel(
          node2.id, relativeAnchorOffset, parentId, selectedFocusOffset);
      selectionController.updateSelection(selection);

      assertEquals(node2.text, selection.anchorNode?.textContent);
      assertEquals(node3.text, selection.focusNode?.textContent);
      assertEquals(relativeAnchorOffset, selection.anchorOffset);
      assertEquals(relativeFocusOffset, selection.focusOffset);
    });
  });
});

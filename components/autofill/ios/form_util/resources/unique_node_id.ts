// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions for assigning and retrieving document-scoped
 * unique IDs on DOM nodes via `ID_SYMBOL` (`__gCrUniqueID`).
 */

import {ID_SYMBOL} from '//components/autofill/ios/form_util/resources/fill_constants.js';

const NEXT_ID_SYMBOL = Symbol.for('__gCrNextUniqueID');

interface IndexableDocument extends Document {
  [NEXT_ID_SYMBOL]: number;
}

interface IndexableNode extends Node {
  [ID_SYMBOL]?: number;
}

const nodeTypeGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'nodeType')?.get;
const ownerDocumentGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'ownerDocument')?.get;

/**
 * Returns the nodeType of `node`. Directly calls the `Node` prototype getter
 * to protect against DOM clobbering (e.g. `<form><input name="nodeType">`).
 */
function safeNodeType(node: Node): number {
  return nodeTypeGetter ? nodeTypeGetter.call(node) : node.nodeType;
}

/**
 * Returns the owner document of the node, or the node itself if it is already
 * a Document. Directly calls the prototype to protect against DOM clobbering
 * and checks `nodeType === Node.DOCUMENT_NODE` to work across same-origin frame
 * realms where `instanceof Document` is false.
 *
 * @param node The node to get the owner document for.
 * @return The owner document of the node, or null if none exists.
 */
export function safeOwnerDocument(node: Node): Document|null {
  if (safeNodeType(node) === Node.DOCUMENT_NODE) {
    return node as Document;
  }
  return ownerDocumentGetter ? ownerDocumentGetter.call(node) :
                               node.ownerDocument;
}

/**
 * Gets or creates a document-scoped unique ID for `node`.
 *
 * @param node The node to get or create an ID for.
 * @return The unique ID for the node (starting at 1).
 */
export function getOrCreateUniqueNodeId(node: Node): number {
  const ownerDoc = (safeOwnerDocument(node) ?? document) as IndexableDocument;
  ownerDoc[NEXT_ID_SYMBOL] ??= 1;

  return ((node as IndexableNode)[ID_SYMBOL] ??= ownerDoc[NEXT_ID_SYMBOL]++);
}

/**
 * Returns whether `node` already has a unique ID stored on `node[ID_SYMBOL]`.
 *
 * @param node The node to check.
 * @return True if `node` already has an assigned unique ID, false otherwise.
 */
export function hasUniqueNodeId(node: Node): boolean {
  return (node as IndexableNode)[ID_SYMBOL] !== undefined;
}

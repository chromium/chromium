// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs readability heuristic on the page and return the result.
(function(allowedVideoRegex) {
try {
  function initialize() {
    // This include will be processed at build time by grit.
    // clang-format off
      // <include src="../../../../third_party/readability/modded_src/Readability.js">
    // clang-format on
    window.Readability = Readability;
  }
  initialize();

  const options = {};
  if (allowedVideoRegex) {
    options.allowedVideoRegex = new RegExp('//' + allowedVideoRegex, 'i');
  }

  /**
   * Deep-clones |node| into |doc|, inlining open shadow roots so that
   * Readability -- which only walks the light DOM -- can see content that the
   * page renders inside web components. <slot> elements are replaced by their
   * assigned nodes so the clone mirrors the flattened tree the user sees.
   *
   * Closed and user-agent shadow roots (<input>, <video>, ...) are not
   * reachable from script, so their contents are necessarily left out. Those
   * hosts report a null shadowRoot and keep their light children, which is
   * what the page renders for them anyway.
   *
   * Recursion depth is bounded by the depth of the flattened tree. A
   * pathologically deep script-built DOM could overflow the stack; the caller
   * catches that and distillation reports no content.
   */
  function cloneFlattenedTree(node, doc) {
    function cloneRecur(node) {
      // These are interface checks rather than localName checks because an SVG
      // or MathML element may also be named "slot" or "template" without
      // implementing the corresponding HTML interface.
      if (node instanceof HTMLTemplateElement) {
        // <template> content lives in a separate fragment that cloneNode()
        // handles natively; importNode(deep) preserves the existing behavior.
        return doc.importNode(node, /*deep=*/ true);
      }

      if (node instanceof HTMLSlotElement) {
        // assignedNodes() resolves fallback content and nested re-projection.
        const assignedNodes = node.assignedNodes({flatten: true});
        if (assignedNodes.length) {
          const fragment = doc.createDocumentFragment();
          for (const assigned of assignedNodes) {
            fragment.appendChild(cloneRecur(assigned));
          }
          return fragment;
        }
        // Empty `assignedNodes` means <slot> is not in a shadow tree: The page
        // renders its children directly, so fall through and clone normally.
      }

      // Shallow import as the children are added below via the flattened tree.
      // A deep import would also copy the light DOM children, causing
      // duplication when the loop below adds children.
      const clone = doc.importNode(node, /*deep=*/ false);
      // A shadow host's light children are only rendered through its slots, so
      // descend into the shadow root rather than into the light children.
      const children =
          node.shadowRoot ? node.shadowRoot.childNodes : node.childNodes;
      for (const child of children) {
        clone.appendChild(cloneRecur(child));
      }
      return clone;
    }
    return cloneRecur(node);
  }

  /**
   * Returns whether any element in the document hosts an open shadow root,
   * i.e. whether cloneFlattenedTree() would differ from a plain deep clone.
   * Shadow roots can be attached to custom elements and to several built-in
   * elements (<div>, <span>, <article>, ...), and no selector matches shadow
   * hosts, so elements are checked one by one until the first host is found.
   */
  function hasOpenShadowRoots() {
    const walker = document.createTreeWalker(document, NodeFilter.SHOW_ELEMENT);
    while (walker.nextNode()) {
      if (walker.currentNode.shadowRoot) {
        return true;
      }
    }
    return false;
  }

  // Fast path: if the document contains no open shadow roots, native
  // document.cloneNode(true) runs entirely in C++ within Blink. This avoids
  // per-node JavaScript recursion and V8 binding overhead for the vast
  // majority of pages that do not use web components.
  let clonedDoc;
  if (!hasOpenShadowRoots()) {
    clonedDoc = document.cloneNode(/*deep=*/ true);
  } else {
    // A shallow clone keeps the document's URL, content type and compatibility
    // mode. Its children (the doctype and <html>) are then cloned with open
    // shadow roots inlined.
    clonedDoc = document.cloneNode(/*deep=*/ false);
    for (const child of document.childNodes) {
      clonedDoc.appendChild(cloneFlattenedTree(child, clonedDoc));
    }
  }
  return new Readability(clonedDoc, options).parse();
} catch (e) {
  window.console.error('Error during distillation: ' + e);
  if (e.stack !== undefined) {
    window.console.error(e.stack);
  }
}
return undefined;
})($$ALLOWED_VIDEO_REGEX);

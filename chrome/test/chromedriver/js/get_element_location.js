// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Constructs new error to be thrown with given code and message.
 *
 * @param {string} message Message reported to user.
 * @param {StatusCode} code StatusCode for error.
 * @return {!Error} Error object that can be thrown.
 */
function newError(message, code) {
  const error = new Error(message);
  error.code = code;
  return error;
}

/**
 * Get the root node for the given element, jumping up through any ShadowRoots
 * if they are found.
 *
 * @param {Node} node The node to find the root of
 * @return {Node} The root node
 */
function getNodeRootThroughAnyShadows(node) {
  // Fetch the root node for the current node.
  let root = node.getRootNode()

  // Keep jumping to the root node for the attachment host of any ShadowRoot.
  while (root.host) {
    root = root.host.getRootNode()
  }

  return root;
}

/**
 * Check whether the specified node is attached to the DOM, either directly or
 * via any attached ShadowRoot.
 *
 * @param {Node} node The node to test
 * @return {boolean} Whether the node is attached to the DOM.
 */
function isNodeReachable(node) {
  const nodeRoot = getNodeRootThroughAnyShadows(node);

  // Check whether the root is the Document or Proxy node.
  return (nodeRoot == document.documentElement.parentNode);
}

function getFirstNonZeroWidthHeightRect(rects) {
  for (const rect of rects) {
    if (rect.height > 0 && rect.width > 0) {
      return rect;
    }
  }
  return rects[0];
}

function getParentRect(element) {
  var parent = element.parentElement;
  var parentRect = getFirstNonZeroWidthHeightRect(parent.getClientRects());
  return parentRect;
}

function getInViewPoint(element) {
  var rectangles = element.getClientRects();
  if (rectangles.length === 0) {
    return false;
  }

  var rect = getFirstNonZeroWidthHeightRect(rectangles);
  var left = Math.max(0, rect.left);
  var right = Math.min(window.innerWidth, rect.right);
  var top = Math.max(0, rect.top);
  var bottom = Math.min(window.innerHeight, rect.bottom);

  // Find the view boundary of the element by checking itself and all of its
  // ancestor's boundary.
  while (element.parentElement != null &&
         element.parentElement != document.body &&
         element.parentElement.getClientRects().length > 0) {
    var parentStyle = window.getComputedStyle(element.parentElement);
    var overflow = parentStyle.getPropertyValue("overflow");
    var overflowX = parentStyle.getPropertyValue("overflow-x");
    var overflowY = parentStyle.getPropertyValue("overflow-y");
    var parentRect = getParentRect(element);
    // Only consider about overflow cases when the parent area overlaps with
    // the element's area.
    if (parentRect.right > left && parentRect.bottom > top &&
        right > parentRect.left && bottom > parentRect.top) {
      if (overflow == "auto" || overflow == "scroll" || overflow == "hidden") {
        left = Math.max(left, parentRect.left);
        right = Math.min(right, parentRect.right);
        top = Math.max(top, parentRect.top);
        bottom = Math.min(bottom, parentRect.bottom);
      } else {
        if (overflowX == "auto" || overflowX == "scroll" ||
            overflowX == "hidden") {
          left = Math.max(left, parentRect.left);
          right = Math.min(right, parentRect.right);
        }
        if (overflowY == "auto" || overflowY == "scroll" ||
            overflowY == "hidden") {
          top = Math.max(top, parentRect.top);
          bottom = Math.min(bottom, parentRect.bottom);
        }
      }
    }
    element = element.parentElement;
  }

  var x = 0.5 * (left + right);
  var y = 0.5 * (top + bottom);
  return [x, y, left, top];
}

function rootNodeIncludes(element, elementPoint) {
  if (!element)
    return false;
  let rootNode = element.getRootNode();
  if (rootNode.elementsFromPoint(elementPoint[0], elementPoint[1])
      .includes(element)) {
    if (rootNode == document)
      return true;
    return rootNodeIncludes(rootNode.host, elementPoint);
  }
  return false;
}

function inView(element) {
  var elementPoint = getInViewPoint(element);
  if (!elementPoint ||
      elementPoint[0] <= 0 || elementPoint[1] <= 0 ||
      elementPoint[0] >= window.innerWidth ||
      elementPoint[1] >= window.innerHeight ||
      !rootNodeIncludes(element, elementPoint)) {
    return false;
  }

  return true;
}

function getElementLocation(element, center) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  if (!isNodeReachable(element)) {
    // errorCode 10: StaleElementException
    throw newError('element is not attached to the page document', 10);
  }

  if (!inView(element)) {
    element.scrollIntoView({behavior: "instant",
                            block: "end",
                            inline: "nearest"});
  }

  var clientRects = element.getClientRects();
  if (clientRects.length === 0) {
    // errorCode 60: ElementNotInteractableException
    throw newError(element + ' has no size and location', 60);
  }

  var elementPoint = getInViewPoint(element);
  if (center) {
    return {
        'x': elementPoint[0],
        'y': elementPoint[1]
    };
  } else {
    return {
        'x': elementPoint[2],
        'y': elementPoint[3]
    };
  }
}

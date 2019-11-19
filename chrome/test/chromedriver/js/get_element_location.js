// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getParentRect(element) {
  var parent = element.parentElement;
  var parentRect = parent.getClientRects()[0];
  return parentRect;
}

function getInViewPoint(element) {
  var rectangles = element.getClientRects();
  if (rectangles.length === 0) {
    return false;
  }

  var rect = rectangles[0];
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

function inView(element) {
  var elementPoint = getInViewPoint(element);
  if (elementPoint[0] <= 0 || elementPoint[1] <= 0 ||
      elementPoint[0] >= window.innerWidth ||
      elementPoint[1] >= window.innerHeight ||
      !document.elementsFromPoint(elementPoint[0], elementPoint[1])
                .includes(element)) {
    return false;
  }

  return true;
}

function getElementLocation(element, center) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  if (!inView(element)) {
    element.scrollIntoView({behavior: "instant",
                            block: "end",
                            inline: "nearest"});
  }

  var clientRects = element.getClientRects();
  if (clientRects.length === 0) {
    var e = new Error(element + ' has no size and location');
    // errorCode 60: ElementNotInteractableException
    e.code = 60;
    throw e;
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

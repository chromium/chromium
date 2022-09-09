// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Return the portion of the element that should be made visible.
// Based on the WebDriver spec, this function only considers the first rectangle
// returned by element.getClientRects function.
// * When the rectangle is already partially visible in the enclosing viewport,
//   return the portion that is currently visible. According to WebDriver spec,
//   no scrolling should be done to bring more of the element into view.
// * When the rectangle is completely outside of the enclosing viewport,
//   return the entire rectangle, as WebDriver spec requires us to scroll the
//   entire rectangle into view. (However, scrolling is NOT the responsibility
//   of this function.)
//
// The returned value is an object with the following properties about the
// region mentioned above: left, top, height, width. Note that left and top are
// relative to the upper-left corner of the element's bounding client rect (as
// returned by element.getBoundingClientRect).
function getElementRegion(element) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  // We try 2 methods to determine element region. Try the first client rect,
  // and then the bounding client rect.
  // SVG is one case that doesn't have a first client rect.
  const clientRects = element.getClientRects();

  // Determines if region is partially in viewport, returning visible region
  // if so. If not, returns null. If fully visible, returns original region.
  function getVisibleSubregion(region) {
    // Given two regions, determines if any intersection occurs.
    // Overlapping edges are not considered intersections.
    function getIntersectingSubregion(region1, region2) {
      if (!(Math.round(region2.right)  <= Math.round(region1.left)   ||
            Math.round(region2.left)   >= Math.round(region1.right)  ||
            Math.round(region2.top)    >= Math.round(region1.bottom) ||
            Math.round(region2.bottom) <= Math.round(region1.top))) {
        // Determines region of intersection.
        // If region2 contains region1, returns region1.
        // If region1 contains region2, returns region2.
        return {
          'left': Math.max(region1.left, region2.left),
          'right': Math.min(region1.right, region2.right),
          'bottom': Math.min(region1.bottom, region2.bottom),
          'top': Math.max(region1.top, region2.top)
        };
      }
      return null;
    }
    const visualViewport = window.visualViewport;
    // We need to disregard any scrollbars therefore instead of innerSize
    // of the window we should use the viewport size.
    // This size can be affected (scaled) by user's pinch.
    // We need to undo this scaling because client rects are calculated
    // relatively to the original unscaled viewport.
    const viewport = new DOMRect(0, 0,
      visualViewport.width * visualViewport.scale,
      visualViewport.height * visualViewport.scale
    );
    return getIntersectingSubregion(viewport, region);
  }

  let boundingRect = null;
  let clientRect = null;
  // Element area of a map has same first ClientRect and BoundingClientRect
  // after blink roll at chromium commit position 290738 which includes blink
  // revision 180610. Thus handle area as a special case.
  if (clientRects.length == 0 || element.tagName.toLowerCase() == 'area') {
    // Area clicking is technically not supported by W3C standard but is a
    // desired feature. Returns region containing the area instead of subregion
    // so that whole area is visible and always clicked correctly.
    if (element.tagName.toLowerCase() == 'area') {
      const coords = element.coords.split(',');
      if (element.shape.toLowerCase() == 'rect') {
        if (coords.length != 4)
          throw new Error('failed to detect the region of the area');
        const leftX = Number(coords[0]);
        const topY = Number(coords[1]);
        const rightX = Number(coords[2]);
        const bottomY = Number(coords[3]);
        return {
            'left': leftX,
            'top': topY,
            'width': rightX - leftX,
            'height': bottomY - topY
        };
      } else if (element.shape.toLowerCase() == 'circle') {
        if (coords.length != 3)
          throw new Error('failed to detect the region of the area');
        const centerX = Number(coords[0]);
        const centerY = Number(coords[1]);
        const radius = Number(coords[2]);
        return {
            'left': Math.max(0, centerX - radius),
            'top': Math.max(0, centerY - radius),
            'width': radius * 2,
            'height': radius * 2
        };
      } else if (element.shape.toLowerCase() == 'poly') {
        if (coords.length < 2)
          throw new Error('failed to detect the region of the area');
        let minX = Number(coords[0]);
        let minY = Number(coords[1]);
        let maxX = minX;
        let maxY = minY;
        for (i = 2; i < coords.length; i += 2) {
          const x = Number(coords[i]);
          const y = Number(coords[i + 1]);
          minX = Math.min(minX, x);
          minY = Math.min(minY, y);
          maxX = Math.max(maxX, x);
          maxY = Math.max(maxY, y);
        }
        return {
            'left': minX,
            'top': minY,
            'width': maxX - minX,
            'height': maxY - minY
        };
      } else {
        throw new Error('shape=' + element.shape + ' is not supported');
      }
    } else {
      clientRect = boundingRect = element.getBoundingClientRect();
    }
  } else {
    boundingRect = element.getBoundingClientRect();
    clientRect = clientRects[0];
    for (let i = 0; i < clientRects.length; i++) {
      if (clientRects[i].height != 0 && clientRects[i].width != 0) {
        clientRect = clientRects[i];
        break;
      }
    }
  }
  const visiblePortion = getVisibleSubregion(clientRect) || clientRect;
  // Returned region is relative to boundingRect's left,top.
  return {
    'left': visiblePortion.left - boundingRect.left,
    'top': visiblePortion.top - boundingRect.top,
    'height': visiblePortion.bottom - visiblePortion.top,
    'width': visiblePortion.right - visiblePortion.left
  };
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getElementRegion(element) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  // We try 2 methods to determine element region. Try the first client rect,
  // and then the bounding client rect.
  // SVG is one case that doesn't have a first client rect.
  var clientRects = element.getClientRects();

  // Determines if region is partially in viewport, returning visible region
  // if so. If not, returns null. If fully visible, returns original region.
  function getVisibleSubregion(region) {
    // Given two regions, determines if any intersection occurs.
    // Overlapping edges are not considered intersections.
    function getIntersectingSubregion(region1, region2) {
      if (!(region2.right  <= region1.left   ||
            region2.left   >= region1.right  ||
            region2.top    >= region1.bottom ||
            region2.bottom <= region1.top)) {
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
    var viewport = new DOMRect(0, 0, window.innerWidth, window.innerHeight);
    return getIntersectingSubregion(viewport, region);
  }

  var boundingRect = null;
  var clientRect = null;
  // Element area of a map has same first ClientRect and BoundingClientRect
  // after blink roll at chromium commit position 290738 which includes blink
  // revision 180610. Thus handle area as a special case.
  if (clientRects.length == 0 || element.tagName.toLowerCase() == 'area') {
    // Area clicking is technically not supported by W3C standard but is a
    // desired feature. Returns region containing the area instead of subregion
    // so that whole area is visible and always clicked correctly.
    if (element.tagName.toLowerCase() == 'area') {
      var coords = element.coords.split(',');
      if (element.shape.toLowerCase() == 'rect') {
        if (coords.length != 4)
          throw new Error('failed to detect the region of the area');
        var leftX = Number(coords[0]);
        var topY = Number(coords[1]);
        var rightX = Number(coords[2]);
        var bottomY = Number(coords[3]);
        return {
            'left': leftX,
            'top': topY,
            'width': rightX - leftX,
            'height': bottomY - topY
        };
      } else if (element.shape.toLowerCase() == 'circle') {
        if (coords.length != 3)
          throw new Error('failed to detect the region of the area');
        var centerX = Number(coords[0]);
        var centerY = Number(coords[1]);
        var radius = Number(coords[2]);
        return {
            'left': Math.max(0, centerX - radius),
            'top': Math.max(0, centerY - radius),
            'width': radius * 2,
            'height': radius * 2
        };
      } else if (element.shape.toLowerCase() == 'poly') {
        if (coords.length < 2)
          throw new Error('failed to detect the region of the area');
        var minX = Number(coords[0]);
        var minY = Number(coords[1]);
        var maxX = minX;
        var maxY = minY;
        for (i = 2; i < coords.length; i += 2) {
          var x = Number(coords[i]);
          var y = Number(coords[i + 1]);
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
      boundingRect = element.getBoundingClientRect();
      clientRect = Object.assign({}, boundingRect);
    }
  } else {
    boundingRect = element.getBoundingClientRect();
    clientRect = clientRects[0];
    for (var i = 0; i < clientRects.length; i++) {
      if (clientRects[i].height != 0 && clientRects[i].width != 0) {
        clientRect = clientRects[i];
        break;
      }
    }
  }
  var visiblePortion = getVisibleSubregion(clientRect) || clientRect;
  // Returned region is relative to boundingRect's left,top.
  return {
    'left': visiblePortion.left - boundingRect.left,
    'top': visiblePortion.top - boundingRect.top,
    'height': visiblePortion.bottom - visiblePortion.top,
    'width': visiblePortion.right - visiblePortion.left
  };
}

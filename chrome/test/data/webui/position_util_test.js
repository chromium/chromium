// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var anchor = document.getElementById('anchor');
var popup = document.getElementById('popup');
var anchorParent = anchor.offsetParent;
var oldGetBoundingClientRect = anchorParent.getBoundingClientRect;
var availRect;

function MockRect(w, h) {
  this.width = w;
  this.height = h;
  this.right = this.left + w;
  this.bottom = this.top + h;
}
MockRect.prototype = {
  left: 0,
  top: 0
};

function setUp() {
  anchor.style.top = '100px';
  anchor.style.left = '100px';
  availRect = new MockRect(200, 200);
  anchorParent.getBoundingClientRect = function() {
    return availRect;
  };
}

function tearDown() {
  document.documentElement.dir = 'ltr';
  anchorParent.getBoundingClientRect = oldGetBoundingClientRect;
}

function testAbovePrimary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);

  assertEquals('auto', popup.style.top);
  assertEquals('100px', popup.style.bottom);

  anchor.style.top = '90px';
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);
  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);
}

function testBelowPrimary() {
  // ensure enough below
  anchor.style.top = '90px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  // ensure not enough below
  anchor.style.top = '100px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);
  assertEquals('auto', popup.style.top);
  assertEquals('100px', popup.style.bottom);
}

function testBeforePrimary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BEFORE);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  anchor.style.left = '90px';
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BEFORE);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testBeforePrimaryRtl() {
  document.documentElement.dir = 'rtl';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  anchor.style.left = '90px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testAfterPrimary() {
  // ensure enough to the right
  anchor.style.left = '90px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  // ensure not enough below
  anchor.style.left = '100px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);
  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);
}

function testAfterPrimaryRtl() {
  document.documentElement.dir = 'rtl';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);

  assertEquals('auto', popup.style.left);
  assertEquals('100px', popup.style.right);

  // ensure not enough below
  anchor.style.left = '90px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);
  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testAboveSecondary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  anchor.style.left = '110px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);

  assertEquals('auto', popup.style.left);
  assertEquals('80px', popup.style.right);
}

function testAboveSecondaryRtl() {
  document.documentElement.dir = 'rtl';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testAboveSecondarySwappedAlign() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE, true);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.ABOVE, true);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testBelowSecondary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.right);

  anchor.style.left = '110px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);

  assertEquals('auto', popup.style.left);
  assertEquals('80px', popup.style.right);
}

function testBelowSecondaryRtl() {
  document.documentElement.dir = 'rtl';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testBelowSecondarySwappedAlign() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW, true);

  assertEquals('auto', popup.style.left);
  assertEquals('90px', popup.style.right);

  anchor.style.left = '80px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BELOW, true);

  assertEquals('80px', popup.style.left);
  assertEquals('auto', popup.style.right);
}

function testBeforeSecondary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BEFORE);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  anchor.style.top = '110px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.BEFORE);

  assertEquals('auto', popup.style.top);
  assertEquals('80px', popup.style.bottom);
}

function testAfterSecondary() {
  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);

  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.bottom);

  anchor.style.top = '110px';

  cr.ui.positionPopupAroundElement(anchor, popup, cr.ui.AnchorType.AFTER);

  assertEquals('auto', popup.style.top);
  assertEquals('80px', popup.style.bottom);
}

function testPositionAtPoint() {
  cr.ui.positionPopupAtPoint(100, 100, popup);

  assertEquals('100px', popup.style.left);
  assertEquals('100px', popup.style.top);
  assertEquals('auto', popup.style.right);
  assertEquals('auto', popup.style.bottom);

  cr.ui.positionPopupAtPoint(100, 150, popup);

  assertEquals('100px', popup.style.left);
  assertEquals('auto', popup.style.top);
  assertEquals('auto', popup.style.right);
  assertEquals('50px', popup.style.bottom);

  cr.ui.positionPopupAtPoint(150, 150, popup);

  assertEquals('auto', popup.style.left);
  assertEquals('auto', popup.style.top);
  assertEquals('50px', popup.style.right);
  assertEquals('50px', popup.style.bottom);
}

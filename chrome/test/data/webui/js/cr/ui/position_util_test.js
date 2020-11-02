// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../../chai_assert.js';
// #import {positionPopupAroundElement, positionPopupAtPoint, AnchorType} from 'chrome://resources/js/cr/ui/position_util.m.js';
// clang-format on

/** @type {!HTMLElement} */
let anchor;
/** @type {!HTMLElement} */
let popup;
let anchorParent;
let oldGetBoundingClientRect;
let availRect;

/**
 * @param {number} w width
 * @param {number} h height
 * @constructor
 */
function MockRect(w, h) {
  /** @type {number} */
  this.left = 0;

  /** @type {number} */
  this.top = 0;

  /** @type {number} */
  this.width = w;

  /** @type {number} */
  this.height = h;

  /** @type {number} */
  this.right = this.left + w;

  /** @type {number} */
  this.bottom = this.top + h;
}

function setUp() {
  document.body.innerHTML = `
    <style>
      html, body {
        margin: 0;
        width: 100%;
        height: 100%;
      }

      #anchor {
        position: absolute;
        width: 10px;
        height: 10px;
        background: green;
      }

      #popup {
        position: absolute;
        top: 0;
        left: 0;
        width: 100px;
        height: 100px;
        background: red;
      }
    </style>

    <div id="anchor"></div>
    <div id="popup"></div>
    `;

  anchor = /** @type {!HTMLElement} */ (document.getElementById('anchor'));
  popup = /** @type {!HTMLElement} */ (document.getElementById('popup'));
  anchorParent = anchor.offsetParent;
  oldGetBoundingClientRect = anchorParent.getBoundingClientRect;

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

Object.assign(window, {
  setUp,
  tearDown,
  testAbovePrimary,
  testBelowPrimary,
  testBeforePrimary,
  testBeforePrimaryRtl,
  testAfterPrimary,
  testAfterPrimaryRtl,
  testAboveSecondary,
  testAboveSecondaryRtl,
  testAboveSecondarySwappedAlign,
  testBelowSecondary,
  testBelowSecondaryRtl,
  testBelowSecondarySwappedAlign,
  testBeforeSecondary,
  testAfterSecondary,
  testPositionAtPoint,
});

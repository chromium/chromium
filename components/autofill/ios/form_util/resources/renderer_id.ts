// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains functions for setting stable renderer IDs to html
 * forms and fields.
 */

import '//components/autofill/ios/form_util/resources/create_fill_namespace.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Extends the Element to add the ability to access its properties
// via the [] notation.
declare interface IndexableElement extends Element {
  [key: symbol]: number;
}

/**
 * Maps elements using their unique ID
 */
const elementMap = new Map();

/**
 * Stores the next available ID for forms and fields. By convention, 0 means
 * null, so we start at 1 and increment from there.
 */
document[gCrWeb.fill.ID_SYMBOL] = 1;

/**
 * @param element Form or form input element.
 */
gCrWeb.fill.setUniqueIDIfNeeded = function(element: IndexableElement): void {
  try {
    const uniqueIDSymbol = gCrWeb.fill.ID_SYMBOL;
    if (typeof element[uniqueIDSymbol] === 'undefined') {
      const elementID = document[uniqueIDSymbol]!++;
      element[uniqueIDSymbol] = elementID;

      //  Store a copy of the ID in the DOM. gCrWeb.fill.getUniqueID will use
      //  the DOM copy when running in the page content world.
      element.setAttribute(
          fillConstants.UNIQUE_ID_ATTRIBUTE, elementID.toString());

      // TODO(crbug.com/40856841): WeakRef starts in 14.5, remove checks once 14
      // is deprecated.
      elementMap.set(
          elementID, window.WeakRef ? new WeakRef(element) : element);
    }
  } catch (e) {
  }
};

/**
 * @param id Unique ID.
 * @return element Form or form input element.
 */
gCrWeb.fill.getElementByUniqueID = function(id: number): Element|null {
  try {
    // TODO(crbug.com/40856841): WeakRef starts in 14.5, remove checks once 14
    // is deprecated.
    return window.WeakRef ? elementMap.get(id).deref() : elementMap.get(id);
  } catch (e) {
    return null;
  }
};

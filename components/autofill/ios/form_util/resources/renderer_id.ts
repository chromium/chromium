// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains functions for setting stable renderer IDs to html
 * forms and fields.
 */

import {ID_SYMBOL, UNIQUE_ID_ATTRIBUTE} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Extends the Element to add the ability to access its properties
// via the [] notation.
declare interface IndexableElement extends Element {
  [key: symbol]: number;
}

/**
 * Maps elements using their unique ID.
 * Since the autofill API is exclusive to the isolated content world,
 * checking for its registration ensures this map is only created there.
 */
if (typeof document.__gCrElementMap === 'undefined' &&
    gCrWeb.hasRegisteredApi('autofill')) {
  document.__gCrElementMap = new Map();
}

/**
 * Stores the next available ID for forms and fields. By convention, 0 means
 * null, so we start at 1 and increment from there.
 */
if (typeof document[ID_SYMBOL] === 'undefined') {
  document[ID_SYMBOL] = 1;
}

/**
 * @param element Form or form input element.
 */
// TODO: (crbug.com/466396701) Add assert to functions called from the page
// content world.
export function setUniqueIDIfNeeded(element: IndexableElement): void {
  try {
    if (typeof element[ID_SYMBOL] === 'undefined') {
      const elementID = document[ID_SYMBOL]!++;
      element[ID_SYMBOL] = elementID;

      //  Store a copy of the ID in the DOM. Utility function getUniqueID will use
      //  the DOM copy when running in the page content world.
      element.setAttribute(UNIQUE_ID_ATTRIBUTE, elementID.toString());

      document.__gCrElementMap.set(elementID, new WeakRef(element));
    }
  } catch (e) {
  }
}

/**
 * @param id Unique ID.
 * @return element Form or form input element.
 */
export function getElementByUniqueID(id: number): Element|null {
  try {
    return document.__gCrElementMap.get(id).deref();
  } catch (e) {
    return null;
  }
}

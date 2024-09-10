// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds functions to the fill namespace that need to be injected
 * both in the page and isolated content worlds.
 */

import '//components/autofill/ios/form_util/resources/create_fill_namespace.js';

import * as fillConstants from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';


/**
 * @param element Form or form input element.
 * @return Unique stable ID converted to string..
 */
gCrWeb.fill.getUniqueID = function(element: any): string {
  // `setUniqueIDIfNeeded` is only available in the isolated content world.
  // Check before invoking it as this script is injected into the page content
  // world as well.
  if (gCrWeb.fill.setUniqueIDIfNeeded) {
    gCrWeb.fill.setUniqueIDIfNeeded(element);
  }

  try {
    const uniqueID = gCrWeb.fill.ID_SYMBOL;
    if (typeof element[uniqueID] !== 'undefined' &&
        !isNaN(element[uniqueID]!)) {
      return element[uniqueID].toString();
    } else {
      return fillConstants.RENDERER_ID_NOT_SET;
    }
  } catch (e) {
    return fillConstants.RENDERER_ID_NOT_SET;
  }
};

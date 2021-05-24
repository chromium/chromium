// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.m.js';

/**
 * @fileoverview Defines the global app state and the actions to change state.
 */

/**
 * @typedef {{
 *   selectedImage: ?chromeos.personalizationApp.mojom.WallpaperImage,
 * }}
 */
export let PersonalizationState;

/**
 * Returns an empty or initial state.
 * @return {!PersonalizationState}
 */
export function emptyState() {
  return {selectedImage: null};
}

/** @enum {string} */
export const ActionName = {
  SET_CURRENT_IMAGE: 'set_current_image',
};

/**
 * Returns an action to set the current image as currently selected across the
 * app. Can be called with null to represent no image currently selected.
 * @param {?chromeos.personalizationApp.mojom.WallpaperImage} image
 * @return {!Action}
 */
export function setCurrentImageAction(image) {
  return {
    image,
    name: ActionName.SET_CURRENT_IMAGE,
  };
}

/**
 * Reducer for personalization app. Must be a pure function that returns a new
 * |PersonalizationState| object. |state| is considered immutable and any
 * changes must return a new object.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 * @param {!PersonalizationState} state
 * @param {!Action} action
 * @return {!PersonalizationState}
 */
export function reduce(state, action) {
  switch (action.name) {
    case ActionName.SET_CURRENT_IMAGE:
      return /** @type {PersonalizationState} */ (
          Object.assign({}, state, {selectedImage: action.image}));
    default:
      console.warn('Unknown action name', action.name);
      return state;
  }
}

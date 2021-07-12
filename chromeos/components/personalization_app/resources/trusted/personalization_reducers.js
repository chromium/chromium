// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Defines reducers for personalization app.  Reducers must be a
 * pure function that returns a new state object if anything has changed.
 * @see [redux tutorial]{@link https://redux.js.org/tutorials/fundamentals/part-3-state-actions-reducers}
 */

import {Action} from 'chrome://resources/js/cr/ui/store.m.js';
import {ActionName} from './personalization_actions.js';

/**
 * @typedef {chromeos.personalizationApp.mojom.WallpaperLayout}
 */
export let WallpaperLayout = chromeos.personalizationApp.mojom.WallpaperLayout;

/**
 * @typedef {chromeos.personalizationApp.mojom.WallpaperType}
 */
export let WallpaperType = chromeos.personalizationApp.mojom.WallpaperType;

/**
 * @typedef {chromeos.personalizationApp.mojom.LocalImage|
 *           chromeos.personalizationApp.mojom.WallpaperImage}
 */
export let DisplayableImage;

/**
 * Stores collections and images from backdrop server.
 * |images| is a mapping of collection id to the list of images.
 * @typedef {{
 *   collections:
 *     ?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>,
 *   images: !Object<string,
 *     ?Array<!chromeos.personalizationApp.mojom.WallpaperImage>>,
 * }}
 */
export let BackdropState;

/**
 * Stores loading state of various components of the app.
 * |images| is a mapping of collection id to loading state.
 * |local| stores data just for local images on disk.
 * |local.data| stores a mapping of stringified UnguessableToken to loading
 * state.
 * |setWallpaper| is true after a user clicks on a wallpaper until the
 * operation completes.
 * @typedef {{
 *   collections: boolean,
 *   images: !Object<string, boolean>,
 *   local: {
 *     images: boolean,
 *     data: !Object<string, boolean>
 *   },
 *   selected: boolean,
 *   setWallpaper: boolean,
 * }}
 */
export let LoadingState;

/**
 * |images| stores the list of images on local disk.
 * |data| stores a mapping of image.id (converted to string) to a thumbnail data
 * url.
 * @typedef {{
 *   images: ?Array<!chromeos.personalizationApp.mojom.LocalImage>,
 *   data: !Object<string, string>,
 * }}
 */
export let LocalState;

/**
 * Top level personalization app state.
 * @typedef {{
 *   backdrop: !BackdropState,
 *   loading: !LoadingState,
 *   local: !LocalState,
 *   selected: ?DisplayableImage,
 * }}
 */
export let PersonalizationState;

/**
 * Returns an empty or initial state.
 * @return {!PersonalizationState}
 */
export function emptyState() {
  return {
    backdrop: {collections: null, images: {}},
    loading: {
      collections: true,
      images: {},
      local: {images: true, data: {}},
      selected: true,
      setWallpaper: false,
    },
    local: {images: null, data: {}},
    selected: null,
  };
}

/**
 * Combines reducers into a single top level reducer. Inspired by Redux's
 * |combineReducers| functions.
 * @param {!Object<string, !Function>} mapping
 * @return {function(!PersonalizationState, !Action): !PersonalizationState}
 */
function combineReducers(mapping) {
  /**
   * @param {!PersonalizationState} state
   * @param {!Action} action
   * @return {!PersonalizationState}
   */
  function reduce(state, action) {
    const newState = Object.keys(mapping).reduce((result, key) => {
      const func = mapping[key];
      result[key] = func(state[key], action);
      return result;
    }, /** @type {!PersonalizationState} */ ({}));
    const change =
        Object.entries(state).some(([key, value]) => newState[key] !== value);
    return change ? newState : state;
  }
  return reduce;
}

/**
 * @param {!BackdropState} state
 * @param {!Action} action
 * @return {!BackdropState}
 */
function backdropReducer(state, action) {
  switch (action.name) {
    case ActionName.SET_COLLECTIONS:
      return {collections: action.collections, images: {}};
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      if (!state.collections.some(({id}) => id === action.collectionId)) {
        console.warn(
            'Cannot store images for unknown collection', action.collectionId);
        return state;
      }
      return /** @type {!BackdropState} */ ({
        ...state,
        images: {...state.images, [action.collectionId]: action.images}
      });
    default:
      return state;
  }
}

/**
 * @param {!LoadingState} state
 * @param {!Action} action
 * @return {!LoadingState}
 */
function loadingReducer(state, action) {
  switch (action.name) {
    case ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS:
      return /** @type {!LoadingState} */ ({
        ...state,
        images: action.collections.reduce(
            (result, {id}) => {
              result[id] = true;
              return result;
            },
            {})
      });
    case ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {...state.local, data: {...state.local.data, [action.id]: true}}
      });
    case ActionName.BEGIN_SELECT_IMAGE:
      return /** @type {!LoadingState} */ ({...state, selected: true});
    case ActionName.SET_COLLECTIONS:
      return /** @type {!LoadingState} */ ({...state, collections: false});
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      return /** @type {!LoadingState} */ ({
        ...state,
        images: {...state.images, [action.collectionId]: false},
      });
    case ActionName.SET_LOCAL_IMAGES:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          ...state.local,
          images: false,
        },
      });
    case ActionName.SET_LOCAL_IMAGE_DATA:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          ...state.local,
          data: {
            ...state.local.data,
            [action.id]: false,
          },
        },
      });
    case ActionName.SET_SELECTED_IMAGE:
      return /** @type {!LoadingState} */ ({...state, selected: false});
    default:
      return state;
  }
}

/**
 * @param {!LocalState} state
 * @param {!Action} action
 * @return {!LocalState}
 */
function localReducer(state, action) {
  switch (action.name) {
    case ActionName.SET_LOCAL_IMAGES:
      return /** @type {!LocalState} */ ({
        ...state,
        images: action.images,
      });
    case ActionName.SET_LOCAL_IMAGE_DATA:
      return /** @type {!LocalState} */ ({
        ...state,
        data: {
          ...state.data,
          [action.id]: action.data,
        }
      });
    default:
      return state;
  }
}

/**
 * @param {?DisplayableImage} state
 * @param {!Action} action
 * @return {?DisplayableImage}
 */
function selectedReducer(state, action) {
  switch (action.name) {
    case ActionName.SET_SELECTED_IMAGE:
      return action.image;
    default:
      return state;
  }
}

const root = combineReducers({
  backdrop: backdropReducer,
  loading: loadingReducer,
  local: localReducer,
  selected: selectedReducer,
});

/**
 * Root level reducer for personalization app.
 * @param {!PersonalizationState} state
 * @param {!Action} action
 * @return {!PersonalizationState}
 */
export function reduce(state, action) {
  return root(state, action);
}

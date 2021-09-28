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
 * @typedef {mojoBase.mojom.FilePath |
 * chromeos.personalizationApp.mojom.WallpaperImage}
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
 *
 * |selected| is a boolean representing the loading state of current wallpaper
 * information. This gets complicated when a user rapidly selects multiple
 * wallpaper images, or picks a new daily refresh wallpaper. This becomes
 * false when a new CurrentWallpaper object is received and the |setImage|
 * counter is at 0.
 *
 * |setImage| is a number representing the number of concurrent requests to set
 * current wallpaper information. This can be more than 1 in case a user rapidly
 * selects multiple wallpaper options.
 * @typedef {{
 *   collections: boolean,
 *   images: !Object<string, boolean>,
 *   local: {
 *     images: boolean,
 *     data: !Object<string, boolean>
 *   },
 *   refreshWallpaper: boolean,
 *   selected: boolean,
 *   setImage: number,
 * }}
 */
export let LoadingState;

/**
 * |images| stores the list of images on local disk.
 * |data| stores a mapping of image.id (converted to string) to a thumbnail data
 * url.
 * @typedef {{
 *   images: ?Array<!mojoBase.mojom.FilePath>,
 *   data: !Object<string, string>,
 * }}
 */
export let LocalState;

/**
 * Stores daily refresh state.
 * @typedef {{
 *   collectionId: ?string
 * }}
 */
export let DailyRefreshState;

/**
 * Top level personalization app state.
 * @typedef {{
 *   backdrop: !BackdropState,
 *   loading: !LoadingState,
 *   local: !LocalState,
 *   currentSelected: ?chromeos.personalizationApp.mojom.CurrentWallpaper,
 *   pendingSelected: ?DisplayableImage,
 *   dailyRefresh: !DailyRefreshState,
 *   error: ?string,
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
      local: {images: false, data: {}},
      refreshWallpaper: false,
      selected: false,
      setImage: 0,
    },
    local: {images: null, data: {}},
    currentSelected: null,
    pendingSelected: null,
    dailyRefresh: {collectionId: null},
    error: null,
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
    case ActionName.BEGIN_LOAD_SELECTED_IMAGE:
      return /** @type {!LoadingState} */ ({...state, selected: true});
    case ActionName.BEGIN_SELECT_IMAGE:
      return /** @type {!LoadingState} */ (
          {...state, setImage: state.setImage + 1});
    case ActionName.END_SELECT_IMAGE:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return /** @type {!LoadingState} */ ({...state, setImage: 0});
      }
      return /** @type {!LoadingState} */ (
          {...state, setImage: state.setImage - 1});
    case ActionName.SET_COLLECTIONS:
      return /** @type {!LoadingState} */ ({...state, collections: false});
    case ActionName.SET_IMAGES_FOR_COLLECTION:
      return /** @type {!LoadingState} */ ({
        ...state,
        images: {...state.images, [action.collectionId]: false},
      });
    case ActionName.BEGIN_LOAD_LOCAL_IMAGES:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          ...state.local,
          images: true,
        },
      });
    case ActionName.SET_LOCAL_IMAGES:
      return /** @type {!LoadingState} */ ({
        ...state,
        local: {
          // Only keep loading state for most recent local images.
          data: (action.images || []).reduce(
              (result, {path}) => {
                if (state.local.data.hasOwnProperty(path)) {
                  result[path] = state.local.data[path];
                }
                return result;
              },
              {}),
          // Image list is done loading.
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
      if (state.setImage === 0) {
        return /** @type {!LoadingState} */ ({...state, selected: false});
      }
      return state;
    case ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return /** @type {!LoadingState} */ ({...state, refreshWallpaper: true});
    case ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE:
      return /** @type {!LoadingState} */ ({...state, refreshWallpaper: false});
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
        // Only keep image thumbnails if the image is still in |images|.
        data: (action.images || []).reduce(
            (result, {path}) => {
              if (state.data.hasOwnProperty(path)) {
                result[path] = state.data[path];
              }
              return result;
            },
            {}),
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
 * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} state
 * @param {!Action} action
 * @return {?chromeos.personalizationApp.mojom.CurrentWallpaper}
 */
function currentSelectedReducer(state, action) {
  switch (action.name) {
    case ActionName.SET_SELECTED_IMAGE:
      return action.image;
    default:
      return state;
  }
}

/**
 * Reducer for the pending selected image. The pendingSelected state is set when
 * a user clicks on an image and before the client code is reached.
 *
 * Note: The pendingSelected state is not cleared when an image is selected i.e.
 * the ActionName.END_SELECT_IMAGE because we allow multiple concurrent requests
 * of selecting images while only keeping the latest pending image and clearing
 * it results in a unwanted jumpy motion of selected state.
 *
 * @param {?DisplayableImage} state
 * @param {!Action} action
 * @return {?DisplayableImage}
 */
function pendingSelectedReducer(state, action) {
  switch (action.name) {
    case ActionName.BEGIN_SELECT_IMAGE:
      return action.image;
    case ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE:
      return null;
    case ActionName.SET_SELECTED_IMAGE:
      const {image} = action;
      if (!image) {
        console.warn('pendingSelectedReducer: Failed to get selected image.');
        return null;
      }
      return state;
    default:
      return state;
  }
}

/**
 * @param {!DailyRefreshState} state
 * @param {!Action} action
 * @returns {!DailyRefreshState}
 */
function dailyRefreshReducer(state, action) {
  switch (action.name) {
    case ActionName.SET_DAILY_REFRESH_COLLECTION_ID:
      return /** @type {!DailyRefreshState} */ ({
        ...state,
        collectionId: action.collectionId,
      });
    default:
      return state;
  }
}

/**
 * @param {?string} state
 * @param {!Action} action
 * @return {?string}
 */
function errorReducer(state, action) {
  switch (action.name) {
    case ActionName.END_SELECT_IMAGE:
      const {success} =
          /** @type {{name: string, success: boolean}} */ (action);
      if (success) {
        return null;
      }
      return state || loadTimeData.getString('setWallpaperError');
    case ActionName.SET_SELECTED_IMAGE:
        const {image} = /** @type {{name: string, image: ?Object}} */(action);
        if (image) {
          return state;
        }
        return state || loadTimeData.getString('loadWallpaperError');
    case ActionName.DISMISS_ERROR:
      if (!state) {
        console.warn(
            'Received dismiss error action when error is already null');
      }
      return null;
    default:
      return state;
  }
}

const root = combineReducers({
  backdrop: backdropReducer,
  loading: loadingReducer,
  local: localReducer,
  currentSelected: currentSelectedReducer,
  pendingSelected: pendingSelectedReducer,
  dailyRefresh: dailyRefreshReducer,
  error: errorReducer,
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

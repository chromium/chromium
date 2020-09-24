// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source privileged WebUI.
 * TODO(b/168274868): Convert this file to ES6.
 */

/** @const */
const helpApp = {};

/**
 * Contains the id and fields that can be searched. Each SearchableItem
 * maps to one Data field in the LSS MOJO API.
 * These originate from the untrusted frame and get parsed by the LSS.
 * @record
 * @struct
 */
helpApp.SearchableItem = function() {};
/**
 * The id of this item.
 * @type {string}
 */
helpApp.SearchableItem.prototype.id;
/**
 * Title text. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.title;
/**
 * Body text. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.body;
/**
 * The main category name, e.g. Perks or Help. Plain localized text.
 * @type {string}
 */
helpApp.SearchableItem.prototype.mainCategoryName;
/**
 * Any sub category name, e.g. a help topic name. Plain localized text.
 * @type {?Array<string>}
 */
helpApp.SearchableItem.prototype.subcategoryNames;
/**
 * Sub headings from the content. Removed from the body text.
 * Plain localized text.
 * @type {?Array<string>}
 */
helpApp.SearchableItem.prototype.subheadings;
/**
 * The locale that this content is localized in.
 * @type {string}
 */
helpApp.SearchableItem.prototype.locale;

/**
 * A position in a string. For highlighting matches in snippets.
 * @record
 * @struct
 */
helpApp.Position = function() {};
/** @type {number} */
helpApp.Position.prototype.length;
/** @type {number} */
helpApp.Position.prototype.start;

/**
 * Response from calling findInSearchIndex.
 * @record
 * @struct
 */
helpApp.SearchResult = function() {};
/** @type {string} */
helpApp.SearchResult.prototype.id;
/**
 * List of positions corresponding to the title. Used in snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.titlePositions;
/**
 * List of positions corresponding to the body. Used in snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.bodyPositions;
/**
 * Index of the most relevant subheading match.
 * @type {?number}
 */
helpApp.SearchResult.prototype.subheadingIndex;
/**
 * List of positions corresponding to the most relevant subheading. Used in
 * snippet.
 * @type {?Array<!helpApp.Position>}
 */
helpApp.SearchResult.prototype.subheadingPositions;

/**
 * Response from calling findInSearchIndex.
 * @record
 * @struct
 */
helpApp.FindResponse = function() {};
/** @type {?Array<!helpApp.SearchResult>} */
helpApp.FindResponse.prototype.results;

/**
 * The delegate which exposes open source privileged WebUi functions to
 * HelpApp.
 * @record
 * @struct
 */
helpApp.ClientApiDelegate = function() {};

/**
 * Opens up the built-in chrome feedback dialog.
 * @return {!Promise<?string>} Promise which resolves when the request has been
 *     acknowledged, if the dialog could not be opened the promise resolves with
 *     an error message, resolves with null otherwise.
 */
helpApp.ClientApiDelegate.prototype.openFeedbackDialog = function() {};

/**
 * Opens up the parental controls section of OS settings.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.showParentalControls = function() {};

/**
 * Add or update the content that is stored in the Search Index.
 * @param {!Array<!helpApp.SearchableItem>} data
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.addOrUpdateSearchIndex = function(data) {};

/**
 * Clear the content that is stored in the Search Index.
 * @return {!Promise<undefined>}
 */
helpApp.ClientApiDelegate.prototype.clearSearchIndex = function() {};

/**
 * Search the search index for content that matches the given query.
 * @param {string} query
 * @return {!Promise<!helpApp.FindResponse>}
 */
helpApp.ClientApiDelegate.prototype.findInSearchIndex = function(query) {};

/**
 * The client Api for interacting with the help app instance.
 * @record
 * @struct
 */
helpApp.ClientApi = function() {};

/**
 * Sets the delegate through which HelpApp can access open-source privileged
 * WebUI methods.
 * @param {!helpApp.ClientApiDelegate} delegate
 */
helpApp.ClientApi.prototype.setDelegate = function(delegate) {};

/**
 * Gets the delegate through which HelpApp can access open-source privileged
 * WebUI methods.
 * @return {!helpApp.ClientApiDelegate}
 */
helpApp.ClientApi.prototype.getDelegate = function() {};

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source unprivileged WebUI.
 * TODO(b/195329580): Convert this file to ES6.
 */

/** @const */
const projectorApp = {};

/**
 * Structure for tool config.
 * @record
 * @struct
 */
projectorApp.AnnotatorToolParams = function() {};

/**
 * The tool type supported in the Ink Engine.
 * @type {string}
 */
projectorApp.AnnotatorToolParams.prototype.tool;

/**
 * The color of the annotator tool.
 * @type {string}
 */
projectorApp.AnnotatorToolParams.prototype.color;

/**
 * The size of the annotator tool.
 * @type {number}
 */
projectorApp.AnnotatorToolParams.prototype.size;

/**
 * The client Api for interacting with the projector annotator instance.
 * @record
 * @struct
 */
projectorApp.AnnotatorApi = function() {};

/**
 * Set the tool parameters in the Ink engine.
 *
 * @param {projectorApp.AnnotatorToolParams} toolParams
 */
projectorApp.AnnotatorApi.prototype.setTool = function(toolParams) {};

/** Undo the most recent operation. */
projectorApp.AnnotatorApi.prototype.undo = function() {};

/** Redo the last undone operation. */
projectorApp.AnnotatorApi.prototype.redo = function() {};

/** Clear the canvas. */
projectorApp.AnnotatorApi.prototype.clear = function() {};

/**
 * Notifies the attached listener when the undo/redo state changes.
 *
 * @param {function(boolean, boolean):undefined} listener The function triggered
 *     when the undo redo status changed. This function takes 2 boolean
 *     arguments: canUndo and canRedo.
 */
projectorApp.AnnotatorApi.prototype.addUndoRedoListener = function(listener) {};

/**
 * Structure for Account information.
 * @record
 * @struct
 */
projectorApp.Account = function() {};

/**
 * The name of the user.
 * @type {string}
 */
projectorApp.Account.prototype.name;

/**
 * The email of the user.
 * @type {string}
 */
projectorApp.Account.prototype.email;

/**
 * The picture url of the user.
 * @type {string}
 */
projectorApp.Account.prototype.pictureURL;

/**
 * Whether this user is a primary user or not.
 * @type {boolean}
 */
projectorApp.Account.prototype.isPrimaryUser;


/**
 * Structure for OAuthToken information passed.
 * @record
 * @struct
 */
projectorApp.OAuthTokenInfo = function() {};

/**
 * The OAuth token.
 * @type {string}
 */
projectorApp.OAuthTokenInfo.prototype.token;

/**
 * The expiration time of the OAuth token.
 * @type {string}
 */
projectorApp.OAuthTokenInfo.prototype.expirationTime;

/**
 * Structure for OAuthToken information passed.
 * @record
 * @struct
 */
projectorApp.OAuthToken = function() {};

/**
 * The email of user associated with the oauth token request.
 * @type {string}
 */
projectorApp.OAuthToken.prototype.email;

/**
 * The OAuth token information.
 * @type {!projectorApp.OAuthTokenInfo}
 */
projectorApp.OAuthToken.prototype.oauthTokenInfo;

/**
 * The error message associated with the token fetch request.
 * @type {string}
 */
projectorApp.OAuthTokenInfo.prototype.error;

/**
 * Structure for Screencast information.
 * @record
 * @struct
 */
projectorApp.PendingScreencast = function() {};

/**
 * The display name of the screencast.
 * @type {string}
 */
projectorApp.PendingScreencast.prototype.name;

/**
 * The upload progress of the screencast. Range from [0, 100).
 * @type {number}
 */
projectorApp.PendingScreencast.prototype.uploadProgress;

// TODO(b/197015567): Add other screencast fields(duration, createdDate etc.).

/**
 * Structure for XHR response.
 * @record
 * @struct
 */
projectorApp.XhrResponse = function() {};

/**
 * True if the request is sent successfully.
 * @type {boolean}
 */
projectorApp.XhrResponse.prototype.success;

/**
 * The response text of the XHR request.
 * @type {string}
 */
projectorApp.XhrResponse.prototype.response;

/**
 * The error message associated with the XHR request.
 * @type {string}
 */
projectorApp.XhrResponse.prototype.error;

/**
 * The delegate interface that the Projector app can use to make requests to
 * chrome.
 * @record
 * @struct
 */
projectorApp.ClientDelegate = function() {};

/**
 * Gets the list of primary and secondary accounts currently available on the
 * device.
 * @return {Promise<Array<!projectorApp.Account>>}
 */
projectorApp.ClientDelegate.prototype.getAccounts = function() {};

/**
 * Checks whether the SWA can trigger a new Projector session.
 * @return {Promise<boolean>}
 */
projectorApp.ClientDelegate.prototype.canStartProjectorSession = function() {};

/**
 * Starts the Projector session if it is possible. Provides the storage
 * directory name where projector output artifacts will be saved in.
 * @param {string} storageDir
 * @return {Promise<boolean>}
 */
projectorApp.ClientDelegate.prototype.startProjectorSession = function(
    storageDir) {};

/**
 * Gets the oauth token with the required scopes for the specified account.
 * @param {string} email user's email
 * @return {!Promise<!projectorApp.OAuthToken>}
 */
projectorApp.ClientDelegate.prototype.getOAuthTokenForAccount = function(
    email) {};

/**
 * Sends 'error' message to handler.
 * The Handler will log the message. If the error is not a recoverable error,
 * the handler closes the corresponding WebUI.
 * @param {!Array<string>} msg Error messages.
 */
projectorApp.ClientDelegate.prototype.onError = function(msg) {};

/**
 * Gets the list of pending screencasts currently available on the device.
 * @return {Promise<Array<!projectorApp.PendingScreencast>>}
 */
projectorApp.ClientDelegate.prototype.getPendingScreencasts = function() {};

/*
 * Send XHR request.
 * @param {string} url the request URL.
 * @param {string} method the request method.
 * @param {string=} requestBody the request body data.
 * @param {boolean=} useCredentials authorize the request with end user
 *  credentials. Used for getting streaming URL.
 * @return {!Promise<!projectorApp.XhrResponse>}
 */
projectorApp.ClientDelegate.prototype.sendXhr = function(
    url, method, requestBody, useCredentials) {};

/**
 * The client Api for interacting with the Projector app instance.
 * @record
 * @struct
 */
projectorApp.AppApi = function() {};

/**
 * Notifies the Projector app that whether it can start a new session.
 * @param {boolean} canStart
 */
projectorApp.AppApi.prototype.onNewScreencastPreconditionChanged = function(
    canStart) {};

/**
 * Notfies the app when screencasts' pending state have changed.
 * @param {!Array<projectorApp.PendingScreencast>} screencasts
 */
projectorApp.AppApi.prototype.onScreencastsStateChange = function(
    screencasts) {};

/**
 * Sets the delegate that the Projector app can use to call into Chrome
 * dependent functions.
 * @param {!projectorApp.ClientDelegate} clientDelegate
 */
projectorApp.AppApi.prototype.setClientDelegate = function(clientDelegate) {};

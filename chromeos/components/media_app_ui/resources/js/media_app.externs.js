// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs file shipped into the chromium build to typecheck uncompiled, "pure"
 * JavaScript used to interoperate with the open-source privileged WebUI.
 * TODO(b/142750452): Convert this file to ES6.
 */

/** @const */
const mediaApp = {};

/**
 * Wraps an HTML File object (or a mock, or media loaded through another means).
 * @record
 * @struct
 */
mediaApp.AbstractFile = function() {};
/**
 * The native Blob representation.
 * @type {!Blob}
 */
mediaApp.AbstractFile.prototype.blob;
/**
 * A name to represent this file in the UI. Usually the filename.
 * @type {string}
 */
mediaApp.AbstractFile.prototype.name;
/**
 * A unique number that represents this file, used to communicate the file in
 * IPC with a parent frame.
 * @type {number|undefined}
 */
mediaApp.AbstractFile.prototype.token;
/**
 * Size of the file, e.g., from the HTML5 File API.
 * @type {number}
 */
mediaApp.AbstractFile.prototype.size;
/**
 * Mime Type of the file, e.g., from the HTML5 File API. Note that the name
 * intentionally does not match the File API version because 'type' is a
 * reserved word in TypeScript.
 * @type {string}
 */
mediaApp.AbstractFile.prototype.mimeType;
/**
 * Whether the file came from the clipboard or a similar in-memory source not
 * backed by a file on disk.
 * @type {boolean|undefined}
 */
mediaApp.AbstractFile.prototype.fromClipboard;
/**
 * An error associated with this file.
 * @type {string|undefined}
 */
mediaApp.AbstractFile.prototype.error;
/**
 * A function that will overwrite the original file with the provided Blob.
 * Returns a promise that resolves when the write operations are complete. Or
 * rejects. Upon success, `size` will reflect the new file size.
 * If null, then in-place overwriting is not supported for this file.
 * Note the "overwrite" may be simulated with a download operation.
 * @type {function(!Blob): !Promise<undefined>|undefined}
 */
mediaApp.AbstractFile.prototype.overwriteOriginal;
/**
 * A function that will delete the original file. Returns a promise that
 * resolves to an enum value (see DeleteResult in chromium message_types)
 * reflecting the result of the deletion. Errors encountered are thrown from the
 * message pipe and handled by invoking functions in Google3.
 * @type {function(): !Promise<number>|undefined}
 */
mediaApp.AbstractFile.prototype.deleteOriginalFile;
/**
 * A function that will rename the original file. Returns a promise that
 * resolves to an enum value (see RenameResult in message_types) reflecting the
 * result of the rename. Errors encountered are thrown from the message pipe and
 * handled by invoking functions in Google3.
 * @type {function(string): !Promise<number>|undefined}
 */
mediaApp.AbstractFile.prototype.renameOriginalFile;
/**
 * A function that will save the provided blob in the file pointed to by
 * pickedFileToken. Once saved the new file takes over this.token and becomes
 * currently writable. The original file is given a new token
 * and pushed forward in the navigation order.
 * @type {function(!Blob, number): !Promise<undefined>|undefined}
 */
mediaApp.AbstractFile.prototype.saveAs;

/**
 * Wraps an HTML FileList object.
 * @record
 * @struct
 */
mediaApp.AbstractFileList = function() {};
/** @type {number} */
mediaApp.AbstractFileList.prototype.length;
/**
 * @param {number} index
 * @return {(null|!mediaApp.AbstractFile)}
 */
mediaApp.AbstractFileList.prototype.item = function(index) {};
/**
 * Returns the file which is currently writable or null if there isn't one.
 * @return {?mediaApp.AbstractFile}
 */
mediaApp.AbstractFileList.prototype.getCurrentlyWritable = function() {};
/**
 * Loads the next file in the navigation order into the media app.
 * @param {number=} currentFileToken the token of the file that is currently
 *     loaded into the media app.
 * @return {!Promise<undefined>}
 */
mediaApp.AbstractFileList.prototype.loadNext = function(currentFileToken) {};
/**
 * Loads the previous file in the navigation order into the media app.
 * @param {number=} currentFileToken the token of the file that is currently
 *     loaded into the media app.
 * @return {!Promise<undefined>}
 */
mediaApp.AbstractFileList.prototype.loadPrev = function(currentFileToken) {};
/**
 * @param {function(!mediaApp.AbstractFileList): void} observer invoked when the
 *     size or contents of the file list changes.
 */
mediaApp.AbstractFileList.prototype.addObserver = function(observer) {};

/**
 * The delegate which exposes open source privileged WebUi functions to
 * MediaApp.
 * @record
 * @struct
 */
mediaApp.ClientApiDelegate = function() {};
/**
 * Opens up the built-in chrome feedback dialog.
 * @return {!Promise<?string>} Promise which resolves when the request has been
 *     acknowledged, if the dialog could not be opened the promise resolves with
 *     an error message, resolves with null otherwise.
 */
mediaApp.ClientApiDelegate.prototype.openFeedbackDialog = function() {};
/**
 * Request for the user to be prompted with a save file dialog. Once the user
 * selects a location a new file handle is created and a new AbstractFile
 * representing that file will be returned. This can be then used in a save as
 * operation. The file extension on `suggestedName` and the provided `mimeType`
 * are used to inform the save as dialog what file should be created. Once the
 * Native Filesystem API allows, this save as dialog will additionally have the
 * filename input be pre-filled with `suggestedName`.
 * TODO(b/161087799): Update function description once Native Filesystem API
 * supports suggestedName.
 * @param {string} suggestedName
 * @param {string} mimeType
 * @return {!Promise<!mediaApp.AbstractFile>}
 */
mediaApp.ClientApiDelegate.prototype.requestSaveFile = function(
    suggestedName, mimeType) {};
/**
 * Request for the user to be prompted with a open file picker. Once the user
 * selects a file, the file is inserted into the navigation order after the
 * current file and navigated to.
 * @return {!Promise<undefined>}
 */
mediaApp.ClientApiDelegate.prototype.openFile = function() {};

/**
 * The client Api for interacting with the media app instance.
 * @record
 * @struct
 */
mediaApp.ClientApi = function() {};
/**
 * Looks up handler(s) and loads media via FileList.
 * @param {!mediaApp.AbstractFileList} files
 * @return {!Promise<undefined>}
 */
mediaApp.ClientApi.prototype.loadFiles = function(files) {};
/**
 * Sets the delegate through which MediaApp can access open-source privileged
 * WebUI methods.
 * @param {?mediaApp.ClientApiDelegate} delegate
 */
mediaApp.ClientApi.prototype.setDelegate = function(delegate) {};

/**
 * Launch data that can be read by the app when it first loads.
 * @type {{files: mediaApp.AbstractFileList}}
 */
window.customLaunchData;

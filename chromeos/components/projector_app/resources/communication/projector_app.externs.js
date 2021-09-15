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
 *
 * See ANNOTATOR_TOOL_TYPE in
 * chrome/apps/projector/client/api/projector_app_message_types.js.
 *
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

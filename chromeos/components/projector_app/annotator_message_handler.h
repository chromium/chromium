// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_
#define CHROMEOS_COMPONENTS_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include "base/callback.h"
#include "base/values.h"

namespace content {
class WebUI;
}

namespace chromeos {

struct AnnotatorTool;

// Callback to setting the tool. The callback allows us to visually show the
// user on whether the tool has changed or not.
using ToolSetCallback = base::RepeatingCallback<void(const AnnotatorTool&)>;

// Callback to notify the user that undo/redo availability has changed. The
// first argument is for undo availability. The second argument to the callback
// is the redo availability.
using UndoRedoAvailabilityCallback = base::RepeatingCallback<void(bool, bool)>;

// Handles communication with the Annotator WebUI (i.e.
// chrome://projector/annotator/annotator_embedder.html)
class AnnotatorMessageHandler : public content::WebUIMessageHandler {
 public:
  AnnotatorMessageHandler();
  AnnotatorMessageHandler(const AnnotatorMessageHandler&) = delete;
  AnnotatorMessageHandler& operator=(const AnnotatorMessageHandler&) = delete;
  ~AnnotatorMessageHandler() override;

  // Public methods exposed to the ash::ProjectorAnnotatorController to control
  // the annotator tool.
  void SetOnToolSetCallback(ToolSetCallback callback);
  void SetUndoRedoAvailabilityCallback(UndoRedoAvailabilityCallback callback);
  void SetTool(const AnnotatorTool& tool);
  void Undo();
  void Redo();
  void Clear();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

 private:
  void OnToolSet(base::Value::ConstListView args);
  void OnUndoRedoAvailabilityChanged(base::Value::ConstListView args);

  // A repeating callback to notify observer that tool has been set.
  ToolSetCallback tool_set_callback_;

  // A repeating callback to notify observer that undo-redo availability has
  // changed.
  UndoRedoAvailabilityCallback undo_redo_availability_callback_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PROJECTOR_APP_ANNOTATOR_MESSAGE_HANDLER_H_

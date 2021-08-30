// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/annotator_message_handler.h"

#include <memory>

#include "annotator_message_handler.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {

const char kToolColor[] = "color";
const char kToolSize[] = "size";
const char kToolType[] = "toolType";

}  // namespace

Tool::Tool() = default;
Tool::Tool(Tool&&) = default;
Tool& Tool::operator=(Tool&&) = default;
Tool::~Tool() = default;

// static
base::Value Tool::ToValue(const Tool& tool) {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetKey(kToolColor, base::Value(tool.color));
  val.SetKey(kToolSize, base::Value(tool.size));
  val.SetKey(kToolType, base::Value(static_cast<int>(tool.type)));
  return val;
}

// static
Tool Tool::ToTool(const base::Value& value) {
  DCHECK(value.is_dict());
  DCHECK(value.FindKey(kToolColor)->is_string());
  DCHECK(value.FindKey(kToolSize)->is_int());
  DCHECK(value.FindKey(kToolType)->is_int());
  Tool t;
  t.color = *(value.FindStringPath(kToolColor));
  t.size = *(value.FindIntPath(kToolSize));
  t.type = static_cast<AnnotatorToolType>(*(value.FindIntPath(kToolType)));
  return t;
}

bool Tool::operator==(const Tool& rhs) const {
  return rhs.color == color && rhs.size == size && rhs.type == type;
}

AnnotatorMessageHandler::AnnotatorMessageHandler() = default;
AnnotatorMessageHandler::~AnnotatorMessageHandler() = default;

void AnnotatorMessageHandler::SetOnToolSetCallback(ToolSetCallback callback) {
  DCHECK(!tool_set_callback_);
  tool_set_callback_ = std::move(callback);
}

void AnnotatorMessageHandler::SetUndoRedoAvailabilityCallback(
    UndoRedoAvailabilityCallback callback) {
  DCHECK(!undo_redo_availability_callback_);
  undo_redo_availability_callback_ = std::move(callback);
}

void AnnotatorMessageHandler::SetTool(const Tool& tool) {
  AllowJavascript();
  FireWebUIListener("setTool", Tool::ToValue(tool));
}

void AnnotatorMessageHandler::Undo() {
  AllowJavascript();
  FireWebUIListener("undo");
}

void AnnotatorMessageHandler::Redo() {
  AllowJavascript();
  FireWebUIListener("redo");
}

void AnnotatorMessageHandler::Clear() {
  AllowJavascript();
  FireWebUIListener("clear");
}

void AnnotatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onToolSet", base::BindRepeating(&AnnotatorMessageHandler::OnToolSet,
                                       base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "OnUndoRedoAvailabilityChanged",
      base::BindRepeating(
          &AnnotatorMessageHandler::OnUndoRedoAvailabilityChanged,
          base::Unretained(this)));
}

void AnnotatorMessageHandler::OnToolSet(base::Value::ConstListView args) {
  if (!tool_set_callback_)
    return;

  DCHECK_EQ(args.size(), 1u);
  tool_set_callback_.Run(Tool::ToTool(args[0]));
}

void AnnotatorMessageHandler::OnUndoRedoAvailabilityChanged(
    base::Value::ConstListView args) {
  if (!undo_redo_availability_callback_)
    return;

  DCHECK_EQ(args.size(), 2u);
  DCHECK(args[0].is_bool());
  DCHECK(args[1].is_bool());
  undo_redo_availability_callback_.Run(args[0].GetBool(), args[1].GetBool());
}

}  // namespace chromeos

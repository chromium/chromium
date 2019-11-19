// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/direct_action.h"

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

DirectAction::DirectAction() = default;
DirectAction::DirectAction(const DirectAction&) = default;
DirectAction::~DirectAction() = default;

DirectAction::DirectAction(const DirectActionProto& proto) {
  for (const std::string& name : proto.names()) {
    names.insert(name);
  }
  for (const std::string& argument : proto.required_arguments()) {
    required_arguments.emplace_back(argument);
  }
  for (const std::string& argument : proto.optional_arguments()) {
    optional_arguments.emplace_back(argument);
  }
}

}  // namespace autofill_assistant

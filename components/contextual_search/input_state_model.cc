// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <vector>

#include "components/contextual_search/contextual_search_session_handle.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

namespace contextual_search {

using omnibox::SearchboxConfig;

InputState::InputState() = default;
InputState::~InputState() = default;

InputStateModel::InputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle,
    const SearchboxConfig& config)
    : session_handle_(session_handle) {
  // TODO(crbug.com/474389216): Implement setting eligibility of
  //   tools and models from the allowed primitives.
}

InputStateModel::~InputStateModel() = default;

base::CallbackListSubscription InputStateModel::subscribe(Subscriber callback) {
  return subscribers_.Add(std::move(callback));
}

void InputStateModel::notifySubscribers() {
  subscribers_.Notify(state_);
}

// TODO(crbug.com/474392676): Implement setting tools and models.
void InputStateModel::setActiveTool(ToolMode tool) {
  updateSelectedState(tool, state_.active_model);
  updateDisabledState();
  notifySubscribers();
}

void InputStateModel::setActiveModel(ModelMode model) {
  updateSelectedState(state_.active_tool, model);
  updateDisabledState();
  notifySubscribers();
}

void InputStateModel::updateSelectedState(ToolMode tool, ModelMode model) {
  state_.active_tool = tool;
  state_.active_model = model;
  // TODO(crbug.com/474390534): Implement updating `state_` for a given
  //   tool and model.
}

void InputStateModel::updateDisabledState() {
  // TODO(crbug.com/474391384): Implement updating which primitives are
  //   disabled based on the current `state_`.
}

}  // namespace contextual_search

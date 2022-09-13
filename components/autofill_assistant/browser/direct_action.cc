// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/direct_action.h"

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

DirectAction::DirectAction() = default;
DirectAction::DirectAction(const DirectAction&) = default;
DirectAction::~DirectAction() = default;

DirectAction::DirectAction(const DirectActionProto& proto)
    : names(proto.names().begin(), proto.names().end()),
      required_arguments(proto.required_arguments().begin(),
                         proto.required_arguments().end()),
      optional_arguments(proto.optional_arguments().begin(),
                         proto.optional_arguments().end()) {}

}  // namespace autofill_assistant

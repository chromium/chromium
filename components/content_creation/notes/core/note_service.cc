// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_service.h"

#include <memory>

#include "base/callback.h"

namespace content_creation {

NoteService::NoteService() = default;

NoteService::~NoteService() = default;

void NoteService::GetTemplates(GetTemplatesCallback callback) {
  // TODO(crbug.com/1194168): Make sure to post the resolution of this callback
  // to prevent rooting any bugs around timing.
  std::move(callback).Run(std::vector<NoteTemplate>());
}

}  // namespace content_creation
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_service.h"

#include "base/callback.h"

namespace content_creation {

NoteService::NoteService(std::unique_ptr<TemplateStore> template_store)
    : template_store_(std::move(template_store)) {}

NoteService::~NoteService() = default;

void NoteService::GetTemplates(GetTemplatesCallback callback) {
  template_store_->GetTemplates(std::move(callback));
}

}  // namespace content_creation
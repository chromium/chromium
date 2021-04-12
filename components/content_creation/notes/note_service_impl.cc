// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/note_service_impl.h"

#include <memory>

#include "base/callback.h"

namespace content_creation {

NoteServiceImpl::NoteServiceImpl() {}

NoteServiceImpl::~NoteServiceImpl() = default;

void NoteServiceImpl::GetTemplates(GetTemplatesCallback callback) {
  std::move(callback).Run(std::vector<NoteTemplate>());
}

}  // namespace content_creation
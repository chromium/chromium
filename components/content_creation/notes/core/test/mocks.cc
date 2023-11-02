// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/test/mocks.h"

namespace content_creation {
namespace test {

MockTemplateStore::MockTemplateStore() : TemplateStore(nullptr, nullptr, "") {}
MockTemplateStore::~MockTemplateStore() = default;

MockNotesRepository::MockNotesRepository() = default;
MockNotesRepository::~MockNotesRepository() = default;

}  // namespace test
}  // namespace content_creation

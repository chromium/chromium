// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note_model_test_utils.h"

#include "base/time/time.h"

namespace user_notes {

std::unique_ptr<UserNoteMetadata> GetTestUserNoteMetadata() {
  base::Time now = base::Time::Now();
  int note_version = 1;
  return std::make_unique<UserNoteMetadata>(now, now, note_version);
}

std::unique_ptr<UserNoteBody> GetTestUserNoteBody() {
  return std::make_unique<UserNoteBody>(u"test note");
}

std::unique_ptr<UserNoteTarget> GetTestUserNotePageTarget(
    const std::string& url) {
  return std::make_unique<UserNoteTarget>(UserNoteTarget::TargetType::kPage,
                                          /*original_text=*/u"", GURL(url),
                                          /*selector=*/"");
}

}  // namespace user_notes

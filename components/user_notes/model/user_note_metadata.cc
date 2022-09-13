// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note_metadata.h"

namespace user_notes {

UserNoteMetadata::UserNoteMetadata(base::Time creation_date,
                                   base::Time modification_date,
                                   int min_note_version)
    : creation_date_(creation_date),
      modification_date_(modification_date),
      min_note_version_(min_note_version) {}

UserNoteMetadata::~UserNoteMetadata() = default;

}  // namespace user_notes

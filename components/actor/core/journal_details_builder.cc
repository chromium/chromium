// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/journal_details_builder.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace actor {

JournalDetailsBuilder::JournalDetailsBuilder() = default;
JournalDetailsBuilder::JournalDetailsBuilder(JournalDetailsBuilder&&) = default;
JournalDetailsBuilder::~JournalDetailsBuilder() = default;

std::string_view JournalEntryTypeToString(mojom::JournalEntryType type) {
  switch (type) {
    case mojom::JournalEntryType::kBegin:
      return "Begin";
    case mojom::JournalEntryType::kEnd:
      return "End";
    case mojom::JournalEntryType::kInstant:
      return "Instant";
  }
  NOTREACHED();
}

std::string TrackToString(uint64_t track_uuid, TaskId task_id) {
  if (MakeFrontEndTrackUUID(task_id) == track_uuid) {
    return "FrontEnd";
  }
  if (MakeBrowserTrackUUID(task_id) == track_uuid) {
    return "Browser";
  }
  if (MakeRendererTrackUUID(task_id) == track_uuid) {
    return "Renderer";
  }
  if (IsGlicExperimentalTriggeringTrack(track_uuid)) {
    return "GlicExperimentalTriggering";
  }
  if (track_uuid != 0) {
    return base::StrCat({"Track ", base::NumberToString(track_uuid)});
  }
  return "";
}

}  // namespace actor

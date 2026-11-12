// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_ai/entity_source_attribution_utils.h"

#include <string>
#include <variant>

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

using GmailSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::GmailSourceMetadata;
using PhotosSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::PhotosSourceMetadata;

std::u16string FormatTimestamp(base::Time timestamp) {
  return timestamp.is_null() ? std::u16string()
                             : base::TimeFormatShortDate(timestamp);
}

}  // namespace

std::u16string GetSourceDisplayTitle(
    const EntityInstance::PersonalContextRecordTypePayload::Source& source) {
  return std::visit(
      absl::Overload{[](const GmailSourceMetadata& gmail) {
                       return !gmail.title.empty()
                                  ? base::UTF8ToUTF16(gmail.title)
                                  : FormatTimestamp(gmail.timestamp);
                     },
                     [](const PhotosSourceMetadata& photos) {
                       return FormatTimestamp(photos.timestamp);
                     }},
      source.metadata);
}

}  // namespace autofill

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_prefs.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace content_creation {
namespace prefs {

namespace {

// Profile pref key used to store and retrieve a list of integer representing
// the order in which to sort the Note templates, via their IDs.
const char kTemplatesRandomOrder[] = "content_creation.notes.random_order";

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kTemplatesRandomOrder);
}

void SetRandomOrder(PrefService* prefs,
                    const std::vector<NoteTemplateIds>& order) {
  DCHECK(prefs);

  base::Value::List list;
  for (size_t i = 0; i < order.size(); i++) {
    list.Append(static_cast<int>(order[i]));
  }

  prefs->SetList(kTemplatesRandomOrder, std::move(list));
}

absl::optional<std::vector<NoteTemplateIds>> TryGetRandomOrder(
    PrefService* prefs) {
  if (!prefs) {
    return absl::nullopt;
  }

  const base::Value& stored_value = prefs->GetValue(kTemplatesRandomOrder);

  if (!stored_value.is_list()) {
    return absl::nullopt;
  }

  std::vector<NoteTemplateIds> template_ids;
  for (const base::Value& current_value : stored_value.GetList()) {
    absl::optional<int> maybe_int = current_value.GetIfInt();
    if (!maybe_int) {
      continue;
    }

    int int_value = maybe_int.value();
    if (int_value < 0 ||
        int_value > static_cast<int>(NoteTemplateIds::kMaxValue)) {
      // The numeric value is not a valid template ID.
      continue;
    }

    template_ids.push_back(static_cast<NoteTemplateIds>(int_value));
  }

  if (template_ids.empty()) {
    return absl::nullopt;
  }

  return template_ids;
}

}  // namespace prefs
}  // namespace content_creation

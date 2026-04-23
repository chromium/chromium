// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_table_model.h"

#include <algorithm>

#include "components/search_engines/template_url_service.h"
#include "components/search_engines/ui_utils.h"

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLService* template_url_service,
    template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids)
    : template_url_service_(template_url_service),
      disabled_starter_pack_ids_(disabled_starter_pack_ids) {
  Reload();
}

TemplateURLTableModel::~TemplateURLTableModel() = default;

void TemplateURLTableModel::Reload() {
  TemplateURL::TemplateURLVector urls =
      template_url_service_->GetTemplateURLs();

  TemplateURL::TemplateURLVector default_entries, active_entries, other_entries,
      extension_entries;
  // Keywords that can be made the default first.
  for (TemplateURL* template_url : urls) {
    if (disabled_starter_pack_ids_.Has(template_url->starter_pack_id())) {
      continue;
    }

    if (template_url_service_->ShowInDefaultList(template_url)) {
      default_entries.push_back(template_url);
    } else if (!template_url_service_->HiddenFromLists(template_url)) {
      if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
        extension_entries.push_back(template_url);
      } else if (template_url_service_->ShowInActivesList(template_url)) {
        active_entries.push_back(template_url);
      } else {
        other_entries.push_back(template_url);
      }
    }
  }

  std::ranges::sort(active_entries,
                    internal::OrderTemplateUrlsByManagedAndAlphabetically());
  std::ranges::sort(other_entries,
                    internal::OrderTemplateUrlsByManagedAndAlphabetically());

  last_search_engine_index_ = default_entries.size();
  last_active_engine_index_ = last_search_engine_index_ + active_entries.size();
  last_other_engine_index_ = last_active_engine_index_ + other_entries.size();

  entries_.clear();
  std::move(default_entries.begin(), default_entries.end(),
            std::back_inserter(entries_));

  std::move(active_entries.begin(), active_entries.end(),
            std::back_inserter(entries_));

  std::move(other_entries.begin(), other_entries.end(),
            std::back_inserter(entries_));

  std::move(extension_entries.begin(), extension_entries.end(),
            std::back_inserter(entries_));
}

TemplateURL* TemplateURLTableModel::GetTemplateURL(size_t index) {
  // Sanity checks for https://crbug.com/40548229.
  CHECK_LT(index, entries_.size());
  CHECK(std::ranges::contains(template_url_service_->GetTemplateURLs(),
                              entries_[index]))
      << "TemplateURLTableModel is returning a pointer to a TemplateURL "
         "that has already been freed by TemplateURLService.";

  return entries_[index];
}

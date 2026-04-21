// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_table_model.h"

#include <algorithm>
#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/ui_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLService* template_url_service,
    template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids)
    : observer_(nullptr),
      template_url_service_(template_url_service),
      disabled_starter_pack_ids_(disabled_starter_pack_ids) {
  DCHECK(template_url_service);
  template_url_service_->AddObserver(this);
  template_url_service_->Load();
  Reload();
}

TemplateURLTableModel::~TemplateURLTableModel() {
  template_url_service_->RemoveObserver(this);
}

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

  if (observer_) {
    observer_->OnModelChanged();
  }
}

size_t TemplateURLTableModel::RowCount() {
  return entries_.size();
}

std::u16string TemplateURLTableModel::GetText(size_t row, int col_id) {
  NOTREACHED();
}

void TemplateURLTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void TemplateURLTableModel::Remove(size_t index) {
  TemplateURL* template_url = GetTemplateURL(index);
  template_url_service_->Remove(template_url);
}

void TemplateURLTableModel::Add(size_t index,
                                const std::u16string& short_name,
                                const std::u16string& keyword,
                                const std::string& url) {
  DCHECK(index <= RowCount());
  DCHECK(!url.empty());
  TemplateURLData data;
  data.SetShortName(short_name);
  data.SetKeyword(keyword);
  data.SetURL(url);
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  template_url_service_->Add(std::make_unique<TemplateURL>(data));
}

void TemplateURLTableModel::ModifyTemplateURL(size_t index,
                                              const std::u16string& title,
                                              const std::u16string& keyword,
                                              const std::string& url) {
  DCHECK(index <= RowCount());
  DCHECK(!url.empty());
  TemplateURL* template_url = GetTemplateURL(index);

  // The default search provider should support replacement.
  DCHECK(template_url_service_->GetDefaultSearchProvider() != template_url ||
         template_url->SupportsReplacement(
             template_url_service_->search_terms_data()));
  template_url_service_->ResetTemplateURL(template_url, title, keyword, url);
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

std::optional<size_t> TemplateURLTableModel::IndexOfTemplateURL(
    const TemplateURL* template_url) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    if (*i == template_url) {
      return static_cast<size_t>(i - entries_.begin());
    }
  }
  return std::nullopt;
}

void TemplateURLTableModel::MakeDefaultTemplateURL(
    size_t index,
    search_engines::ChoiceMadeLocation choice_location) {
  DCHECK_LT(index, RowCount());

  TemplateURL* keyword = GetTemplateURL(index);
  const TemplateURL* current_default =
      template_url_service_->GetDefaultSearchProvider();
  if (current_default == keyword) {
    return;
  }

  template_url_service_->SetUserSelectedDefaultSearchProvider(keyword,
                                                              choice_location);
}

void TemplateURLTableModel::SetIsActiveTemplateURL(size_t index,
                                                   bool is_active) {
  DCHECK(index <= RowCount());
  TemplateURL* keyword = GetTemplateURL(index);

  template_url_service_->SetIsActiveTemplateURL(keyword, is_active);
}

void TemplateURLTableModel::OnTemplateURLServiceChanged() {
  Reload();
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_table_model.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLService* template_url_service)
    : observer_(nullptr), template_url_service_(template_url_service) {
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
  for (auto* template_url : urls) {
    // Don't include starter pack keywords if the starter pack feature flag is
    // not enabled.
    if (!OmniboxFieldTrial::IsSiteSearchStarterPackEnabled() &&
        template_url->starter_pack_id() != 0) {
      continue;
    }

    if (template_url_service_->ShowInDefaultList(template_url)) {
      default_entries.push_back(template_url);
    } else if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
      extension_entries.push_back(template_url);
    } else if (template_url->is_active() ==
               TemplateURLData::ActiveStatus::kTrue) {
      active_entries.push_back(template_url);
    } else {
      other_entries.push_back(template_url);
    }
  }

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

  if (observer_)
    observer_->OnModelChanged();
}

size_t TemplateURLTableModel::RowCount() {
  return entries_.size();
}

std::u16string TemplateURLTableModel::GetText(size_t row, int col_id) {
  DCHECK(row < RowCount());
  const TemplateURL* url = entries_[row];
  if (col_id == IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN) {
    std::u16string url_short_name = url->short_name();
    // TODO(xji): Consider adding a special case if the short name is a URL,
    // since those should always be displayed LTR. Please refer to
    // http://crbug.com/6726 for more information.
    base::i18n::AdjustStringForLocaleDirection(&url_short_name);
    return (template_url_service_->GetDefaultSearchProvider() == url) ?
        l10n_util::GetStringFUTF16(IDS_SEARCH_ENGINES_EDITOR_DEFAULT_ENGINE,
                                   url_short_name) : url_short_name;
  }

  DCHECK_EQ(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN, col_id);
  // Keyword should be domain name. Force it to have LTR directionality.
  return base::i18n::GetDisplayStringInLTRDirectionality(url->keyword());
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
  // Sanity checks for https://crbug.com/781703.
  CHECK_LT(index, entries_.size());
  CHECK(
      base::Contains(template_url_service_->GetTemplateURLs(), entries_[index]))
      << "TemplateURLTableModel is returning a pointer to a TemplateURL "
         "that has already been freed by TemplateURLService.";

  return entries_[index];
}

absl::optional<size_t> TemplateURLTableModel::IndexOfTemplateURL(
    const TemplateURL* template_url) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    if (*i == template_url)
      return static_cast<size_t>(i - entries_.begin());
  }
  return absl::nullopt;
}

void TemplateURLTableModel::MakeDefaultTemplateURL(size_t index) {
  DCHECK_LT(index, RowCount());

  TemplateURL* keyword = GetTemplateURL(index);
  const TemplateURL* current_default =
      template_url_service_->GetDefaultSearchProvider();
  if (current_default == keyword)
    return;

  template_url_service_->SetUserSelectedDefaultSearchProvider(keyword);
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

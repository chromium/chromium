// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_table_model.h"

#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLService* template_url_service)
    : observer_(NULL), template_url_service_(template_url_service) {
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

  TemplateURL::TemplateURLVector default_entries, other_entries,
      extension_entries;
  // Keywords that can be made the default first.
  for (auto* template_url : urls) {
    if (template_url_service_->ShowInDefaultList(template_url))
      default_entries.push_back(template_url);
    else if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)
      extension_entries.push_back(template_url);
    else
      other_entries.push_back(template_url);
  }

  last_search_engine_index_ = static_cast<int>(default_entries.size());
  last_other_engine_index_ = last_search_engine_index_ +
      static_cast<int>(other_entries.size());

  entries_.clear();
  std::move(default_entries.begin(), default_entries.end(),
            std::back_inserter(entries_));

  std::move(other_entries.begin(), other_entries.end(),
            std::back_inserter(entries_));

  std::move(extension_entries.begin(), extension_entries.end(),
            std::back_inserter(entries_));

  if (observer_)
    observer_->OnModelChanged();
}

int TemplateURLTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

base::string16 TemplateURLTableModel::GetText(int row, int col_id) {
  DCHECK(row >= 0 && row < RowCount());
  const TemplateURL* url = entries_[row];
  if (col_id == IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN) {
    base::string16 url_short_name = url->short_name();
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

void TemplateURLTableModel::Remove(int index) {
  TemplateURL* template_url = GetTemplateURL(index);
  template_url_service_->Remove(template_url);
}

void TemplateURLTableModel::Add(int index,
                                const base::string16& short_name,
                                const base::string16& keyword,
                                const std::string& url) {
  DCHECK(index >= 0 && index <= RowCount());
  DCHECK(!url.empty());
  TemplateURLData data;
  data.SetShortName(short_name);
  data.SetKeyword(keyword);
  data.SetURL(url);
  template_url_service_->Add(std::make_unique<TemplateURL>(data));
}

void TemplateURLTableModel::ModifyTemplateURL(int index,
                                              const base::string16& title,
                                              const base::string16& keyword,
                                              const std::string& url) {
  DCHECK(index >= 0 && index <= RowCount());
  DCHECK(!url.empty());
  TemplateURL* template_url = GetTemplateURL(index);

  // The default search provider should support replacement.
  DCHECK(template_url_service_->GetDefaultSearchProvider() != template_url ||
         template_url->SupportsReplacement(
             template_url_service_->search_terms_data()));
  template_url_service_->ResetTemplateURL(template_url, title, keyword, url);
}

TemplateURL* TemplateURLTableModel::GetTemplateURL(int index) {
  // Sanity checks for https://crbug.com/781703.
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), entries_.size());
  CHECK(
      base::Contains(template_url_service_->GetTemplateURLs(), entries_[index]))
      << "TemplateURLTableModel is returning a pointer to a TemplateURL "
         "that has already been freed by TemplateURLService.";

  return entries_[index];
}

int TemplateURLTableModel::IndexOfTemplateURL(
    const TemplateURL* template_url) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    if (*i == template_url)
      return static_cast<int>(i - entries_.begin());
  }
  return -1;
}

void TemplateURLTableModel::MakeDefaultTemplateURL(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, RowCount());

  TemplateURL* keyword = GetTemplateURL(index);
  const TemplateURL* current_default =
      template_url_service_->GetDefaultSearchProvider();
  if (current_default == keyword)
    return;

  template_url_service_->SetUserSelectedDefaultSearchProvider(keyword);
}

void TemplateURLTableModel::OnTemplateURLServiceChanged() {
  Reload();
}

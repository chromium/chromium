// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/base/models/table_model.h"

class TemplateURL;
class TemplateURLService;

namespace search_engines {
enum class ChoiceMadeLocation;
}

// TemplateURLTableModel is the TableModel implementation used by
// KeywordEditorView to show the keywords in a TableView.
//
// TemplateURLTableModel has two columns, the first showing the description,
// the second the keyword.
//
// TemplateURLTableModel maintains a vector of TemplateURLs. The entries in the
// model are sorted such that non-generated keywords appear first (grouped
// together) and are followed by generated keywords.

class TemplateURLTableModel : public ui::TableModel,
                                     TemplateURLServiceObserver {
 public:
  explicit TemplateURLTableModel(TemplateURLService* template_url_service);

  TemplateURLTableModel(const TemplateURLTableModel&) = delete;
  TemplateURLTableModel& operator=(const TemplateURLTableModel&) = delete;

  ~TemplateURLTableModel() override;

  // Reloads the entries from the TemplateURLService. This should ONLY be
  // invoked if the TemplateURLService wasn't initially loaded and has been
  // loaded.
  void Reload();

  // ui::TableModel overrides.
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // Returns the keyword to display corresponding to the search engine in `row`.
  std::u16string GetKeywordToDisplay(size_t row);

  // Removes the entry at the specified index.
  void Remove(size_t index);

  // Adds a new entry at the specified index.
  void Add(size_t index,
           const std::u16string& short_name,
           const std::u16string& keyword,
           const std::string& url);

  // Update the entry at the specified index.
  void ModifyTemplateURL(size_t index,
                         const std::u16string& title,
                         const std::u16string& keyword,
                         const std::string& url);

  // Reloads the icon at the specified index.
  void ReloadIcon(size_t index);

  // Returns the TemplateURL at the specified index.
  TemplateURL* GetTemplateURL(size_t index);

  // Returns the index of the TemplateURL, or nullopt if it the TemplateURL is
  // not found.
  std::optional<size_t> IndexOfTemplateURL(const TemplateURL* template_url);

  // Make the TemplateURL at |index| the default.  Returns the new index, or -1
  // if the index is invalid or it is already the default.
  void MakeDefaultTemplateURL(
      size_t index,
      search_engines::ChoiceMadeLocation choice_location);

  // Activates the TemplateURL at the specified index if `is_active` is true and
  // deactivates if false. When the TemplateURL is active, it can be invoked by
  // keyword via the omnibox.
  void SetIsActiveTemplateURL(size_t index, bool is_active);

  // Returns the index of the last entry shown in the search engines group.
  size_t last_search_engine_index() const { return last_search_engine_index_; }

  // Returns the index of the last entry shown in the active search engines
  // group.
  size_t last_active_engine_index() const { return last_active_engine_index_; }

  // Returns the index of the last entry shown in the other search engines
  // group.
  size_t last_other_engine_index() const { return last_other_engine_index_; }

 private:
  // TemplateURLServiceObserver notification.
  void OnTemplateURLServiceChanged() override;

  raw_ptr<ui::TableModelObserver> observer_;

  // The entries.
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> entries_;

  // The model we're displaying entries from.
  raw_ptr<TemplateURLService> template_url_service_;

  // Index of the last search engine in entries_. This is used to determine the
  // group boundaries.
  size_t last_search_engine_index_;

  // Index of the last active engine in entries_. Engines are active if they've
  // been used or manually added/modified by the user. This is used to determine
  // the group boundaries.
  size_t last_active_engine_index_;

  // Index of the last other engine in entries_. This is used to determine the
  // group boundaries.
  size_t last_other_engine_index_;
};


#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

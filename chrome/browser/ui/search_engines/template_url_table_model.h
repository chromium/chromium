// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/base/models/table_model.h"

class TemplateURL;
class TemplateURLService;

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

  ~TemplateURLTableModel() override;

  // Reloads the entries from the TemplateURLService. This should ONLY be
  // invoked if the TemplateURLService wasn't initially loaded and has been
  // loaded.
  void Reload();

  // ui::TableModel overrides.
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // Removes the entry at the specified index.
  void Remove(int index);

  // Adds a new entry at the specified index.
  void Add(int index,
           const base::string16& short_name,
           const base::string16& keyword,
           const std::string& url);

  // Update the entry at the specified index.
  void ModifyTemplateURL(int index,
                         const base::string16& title,
                         const base::string16& keyword,
                         const std::string& url);

  // Reloads the icon at the specified index.
  void ReloadIcon(int index);

  // Returns the TemplateURL at the specified index.
  TemplateURL* GetTemplateURL(int index);

  // Returns the index of the TemplateURL, or -1 if it the TemplateURL is not
  // found.
  int IndexOfTemplateURL(const TemplateURL* template_url);

  // Make the TemplateURL at |index| the default.  Returns the new index, or -1
  // if the index is invalid or it is already the default.
  void MakeDefaultTemplateURL(int index);

  // Returns the index of the last entry shown in the search engines group.
  int last_search_engine_index() const { return last_search_engine_index_; }

  // Returns the index of the last entry shown in the other search engines
  // group.
  int last_other_engine_index() const { return last_other_engine_index_; }

 private:
  // TemplateURLServiceObserver notification.
  void OnTemplateURLServiceChanged() override;

  ui::TableModelObserver* observer_;

  // The entries.
  std::vector<TemplateURL*> entries_;

  // The model we're displaying entries from.
  TemplateURLService* template_url_service_;

  // Index of the last search engine in entries_. This is used to determine the
  // group boundaries.
  int last_search_engine_index_;

  // Index of the last other engine in entries_. This is used to determine the
  // group boundaries.
  int last_other_engine_index_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLTableModel);
};


#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

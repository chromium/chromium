// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_starter_pack_data.h"

class TemplateURL;
class TemplateURLService;

// TemplateURLTableModel is used to show the keywords in a TableView. It has two
// columns, the first showing the description, the second the keyword.
//
// TemplateURLTableModel maintains a vector of TemplateURLs. The entries in the
// model are sorted such that non-generated keywords appear first (grouped
// together) and are followed by generated keywords.
// TODO (crbug.com/494551138): Remove once `SearchSettingsUpdate` is launched.
class TemplateURLTableModel {
 public:
  TemplateURLTableModel(TemplateURLService* template_url_service,
                        template_url_starter_pack_data::StarterPackIdSet
                            disabled_starter_pack_ids);

  TemplateURLTableModel(const TemplateURLTableModel&) = delete;
  TemplateURLTableModel& operator=(const TemplateURLTableModel&) = delete;

  ~TemplateURLTableModel();

  // Reloads the entries from the TemplateURLService. This should ONLY be
  // invoked if the TemplateURLService wasn't initially loaded and has been
  // loaded.
  void Reload();

  // Returns the TemplateURL at the specified index.
  TemplateURL* GetTemplateURL(size_t index);

  // Returns the index of the last entry shown in the search engines group.
  size_t last_search_engine_index() const { return last_search_engine_index_; }

  // Returns the index of the last entry shown in the active search engines
  // group.
  size_t last_active_engine_index() const { return last_active_engine_index_; }

  // Returns the index of the last entry shown in the other search engines
  // group.
  size_t last_other_engine_index() const { return last_other_engine_index_; }

  size_t engine_count() const { return entries_.size(); }

 private:
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

  // Contains the starter pack ids that should not be included in the table.
  template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

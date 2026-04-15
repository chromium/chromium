// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/search_engines/template_url_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;
class TemplateURL;
class TemplateURLService;
class TemplateURLTableModel;

namespace search_engines {
enum class ChoiceMadeLocation;
}

class KeywordEditorController {
 public:
  explicit KeywordEditorController(Profile* profile);

  KeywordEditorController(const KeywordEditorController&) = delete;
  KeywordEditorController& operator=(const KeywordEditorController&) = delete;

  ~KeywordEditorController();

  // Invoked when the user successfully fills out the add keyword dialog.
  // Propagates the change to the TemplateURLService and updates the table
  // model. Returns the id of the added URL.
  TemplateURLID AddTemplateURL(const std::u16string& title,
                               const std::u16string& keyword,
                               const std::string& fixed_up_url);

  // Invoked when the user modifies a TemplateURL. Updates the
  // TemplateURLService and table model appropriately.
  void ModifyTemplateURL(TemplateURL* template_url,
                         const std::u16string& title,
                         const std::u16string& keyword,
                         const std::string& fixed_up_url);

  // Return true if the given |url| can be edited.
  bool CanEdit(const TemplateURL* url) const;

  // Return true if the given |url| can be made the default.
  bool CanMakeDefault(const TemplateURL* url) const;

  // Return true if the given `url` can be removed. A `url` can be removed if it
  // is a normal entry (non-extension) and is not the current default search
  // engine or a starter pack engine.
  bool CanRemove(const TemplateURL* url) const;

  // Return true if the given `url` can be activated. A `url` can be activated
  // if it is currently inactive and is not a prepopulated engine.
  bool CanActivate(const TemplateURL* url) const;

  // Return true if the given `url` can be deactivated. A `url` can be
  // deactivated if it is currently active and is not a prepopulated engine or
  // the current default search engine.
  bool CanDeactivate(const TemplateURL* url) const;

  // Return true if the user should be asked to confirm before removing the
  // given `url`.
  bool ShouldConfirmRemoval(const TemplateURL* url) const;

  // Return true if a search engine is managed by policy.
  bool IsManaged(const TemplateURL* url) const;

  // Remove the TemplateURL with the specified id.
  void RemoveTemplateURL(TemplateURLID id);

  // Returns the default search provider.
  const TemplateURL* GetDefaultSearchProvider();

  // Make the TemplateURL with the specified id the default search provider.
  void MakeDefaultTemplateURL(
      TemplateURLID id,
      search_engines::ChoiceMadeLocation choice_location);

  // Activates the TemplateURL with the specified id if `is_active` is true or
  // deactivates it if false.
  void SetIsActiveTemplateURL(TemplateURLID id, bool is_active);

  // Return true if the |url_model_| data is loaded.
  bool loaded() const;

  // Return the TemplateURL corresponding to the `id`. Returns
  // `nullptr` if the id was not found.
  TemplateURL* GetTemplateURL(TemplateURLID id);

  // Return the TemplateURL corresponding to the `index` in the table model.
  TemplateURL* GetTemplateURLForIndex(int index);

  TemplateURLTableModel* table_model() { return table_model_.get(); }

  void UpdateIdToTemplateURLMapping();

 private:
  raw_ptr<TemplateURLService> url_model_;

  // Mapping from IDs to the TemplateURL.
  absl::flat_hash_map<TemplateURLID, TemplateURL*> id_to_turl_;

  // Model for the TableView.
  std::unique_ptr<TemplateURLTableModel> table_model_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_

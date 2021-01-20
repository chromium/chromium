// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

class Profile;
class TemplateURL;
class TemplateURLService;
class TemplateURLTableModel;

class KeywordEditorController {
 public:
  explicit KeywordEditorController(Profile* profile);
  ~KeywordEditorController();

  // Invoked when the user succesfully fills out the add keyword dialog.
  // Propagates the change to the TemplateURLService and updates the table
  // model.  Returns the index of the added URL.
  int AddTemplateURL(const base::string16& title,
                     const base::string16& keyword,
                     const std::string& url);

  // Invoked when the user modifies a TemplateURL. Updates the
  // TemplateURLService and table model appropriately.
  void ModifyTemplateURL(TemplateURL* template_url,
                         const base::string16& title,
                         const base::string16& keyword,
                         const std::string& url);

  // Return true if the given |url| can be edited.
  bool CanEdit(const TemplateURL* url) const;

  // Return true if the given |url| can be made the default.
  bool CanMakeDefault(const TemplateURL* url) const;

  // Return true if the given |url| can be removed.
  bool CanRemove(const TemplateURL* url) const;

  // Remove the TemplateURL at the specified index in the TableModel.
  void RemoveTemplateURL(int index);

  // Returns the default search provider.
  const TemplateURL* GetDefaultSearchProvider();

  // Make the TemplateURL at the specified index (into the TableModel) the
  // default search provider.
  void MakeDefaultTemplateURL(int index);

  // Return true if the |url_model_| data is loaded.
  bool loaded() const;

  // Return the TemplateURL corresponding to the |index| in the model.
  TemplateURL* GetTemplateURL(int index);

  TemplateURLTableModel* table_model() {
    return table_model_.get();
  }

 private:
  TemplateURLService* url_model_;

  // Model for the TableView.
  std::unique_ptr<TemplateURLTableModel> table_model_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_

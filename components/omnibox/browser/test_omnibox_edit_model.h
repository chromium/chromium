// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/prefs/testing_pref_service.h"

class TestOmniboxEditModel : public OmniboxEditModel {
 public:
  TestOmniboxEditModel(OmniboxController* omnibox_controller,
                       OmniboxView* view,
                       PrefService* pref_service);
  ~TestOmniboxEditModel() override;
  TestOmniboxEditModel(const TestOmniboxEditModel&) = delete;
  TestOmniboxEditModel& operator=(const TestOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  bool PopupIsOpen() const override;
  AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const override;

  void SetPopupIsOpen(bool open);

  void SetCurrentMatchForTest(const AutocompleteMatch& match);

  void OnPopupDataChanged(const std::u16string& temporary_text,
                          bool is_temporary_text,
                          const std::u16string& inline_autocompletion,
                          const std::u16string& prefix_autocompletion,
                          const std::u16string& keyword,
                          const std::u16string& keyword_placeholder,
                          bool is_keyword_hint,
                          const std::u16string& additional_text,
                          const AutocompleteMatch& match) override;

  bool HasTemporaryText() { return has_temporary_text_; }

  const std::u16string& text() const { return text_; }
  bool is_temporary_text() const { return is_temporary_text_; }

 protected:
  PrefService* GetPrefService() override;
  const PrefService* GetPrefService() const override;

 private:
  bool popup_is_open_;
  std::unique_ptr<AutocompleteMatch> override_current_match_;

  // Contains the most recent text passed by the popup model to the edit model.
  std::u16string text_;
  bool is_temporary_text_ = false;
  raw_ptr<PrefService> pref_service_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_MODEL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_LANGUAGE_SEARCH_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_LANGUAGE_SEARCH_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Textfield;
class ScrollView;
}  // namespace views

class TranslateBubbleModel;

class TranslateLanguageSearchView : public views::View,
                                    public views::TextfieldController {
  METADATA_HEADER(TranslateLanguageSearchView, views::View)

 public:
  TranslateLanguageSearchView(
      TranslateBubbleModel* model,
      const std::vector<std::string>& recent_target_codes,
      base::RepeatingCallback<void(int)> on_language_selected);
  TranslateLanguageSearchView(const TranslateLanguageSearchView&) = delete;
  TranslateLanguageSearchView& operator=(const TranslateLanguageSearchView&) =
      delete;
  ~TranslateLanguageSearchView() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  void ResetLanguageIndex(int language_index);
  void CreateLanguageHoverButton(int language_index);

 private:
  void UpdateLanguageList(const std::u16string& query);
  void OnLanguageButtonPressed(int language_index);
  void ClearLanguageList();

  // Helper method to get the index of a language by its code.

  raw_ptr<TranslateBubbleModel> model_;
  std::vector<std::string> recent_target_codes_;
  base::RepeatingCallback<void(int)> on_language_selected_;

  raw_ptr<views::Textfield> search_field_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> list_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_LANGUAGE_SEARCH_VIEW_H_

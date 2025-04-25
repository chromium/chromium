// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_MODEL_H_

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/action_id.h"

namespace page_actions {

class MockPageActionModel : public PageActionModelInterface {
 public:
  MockPageActionModel();
  ~MockPageActionModel() override;

  MOCK_METHOD(bool, GetVisible, (), (const, override));
  MOCK_METHOD(bool, GetShowSuggestionChip, (), (const, override));
  MOCK_METHOD(bool, GetShouldAnimateChip, (), (const, override));
  MOCK_METHOD(bool, GetShouldAnnounceChip, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetText, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetAccessibleName, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetTooltipText, (), (const, override));
  MOCK_METHOD(const ui::ImageModel&, GetImage, (), (const, override));
  MOCK_METHOD(bool, GetActionItemIsShowingBubble, (), (const, override));
  MOCK_METHOD(void,
              AddObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              SetActionItemProperties,
              (base::PassKey<PageActionController>,
               const actions::ActionItem* action_item),
              (override));
  MOCK_METHOD(void,
              SetShowRequested,
              (base::PassKey<PageActionController>, bool requested),
              (override));
  MOCK_METHOD(void,
              SetShowSuggestionChip,
              (base::PassKey<PageActionController>, bool show),
              (override));
  MOCK_METHOD(void,
              SetSuggestionChipConfig,
              (base::PassKey<PageActionController>,
               const SuggestionChipConfig& config),
              (override));
  MOCK_METHOD(void,
              SetHasPinnedIcon,
              (base::PassKey<PageActionController>, bool has_pinned_icon),
              (override));
  MOCK_METHOD(void,
              SetTabActive,
              (base::PassKey<PageActionController>, bool is_active),
              (override));
  MOCK_METHOD(void,
              SetOverrideText,
              (base::PassKey<PageActionController>,
               const std::optional<std::u16string>& text),
              (override));
  MOCK_METHOD(void,
              SetOverrideAccessibleName,
              (base::PassKey<PageActionController>,
               const std::optional<std::u16string>& override_accessible_name),
              (override));
  MOCK_METHOD(void,
              SetOverrideImage,
              (base::PassKey<PageActionController>,
               const std::optional<ui::ImageModel>& override_text),
              (override));
  MOCK_METHOD(void,
              SetOverrideTooltip,
              (base::PassKey<PageActionController>,
               const std::optional<std::u16string>& override_tooltip),
              (override));
  MOCK_METHOD(void,
              SetShouldHidePageAction,
              (base::PassKey<PageActionController>, bool should_hide),
              (override));
  MOCK_METHOD(bool, IsEphemeral, (), (const, override));
};

template <typename PageActionModelType>
class FakePageActionModelFactory : public PageActionModelFactory {
 public:
  std::unique_ptr<PageActionModelInterface> Create(
      int action_id,
      bool /*is_ephemeral*/) override {
    auto model = std::make_unique<PageActionModelType>();
    model_map_.emplace(action_id, model.get());
    return model;
  }

  // Model getter for tests to set expectations.
  PageActionModelType& Get(int action_id) {
    auto id_to_model = model_map_.find(action_id);
    CHECK(id_to_model != model_map_.end());
    CHECK_NE(id_to_model->second, nullptr);
    return *id_to_model->second;
  }

 private:
  std::map<actions::ActionId, PageActionModelType*> model_map_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_MODEL_H_

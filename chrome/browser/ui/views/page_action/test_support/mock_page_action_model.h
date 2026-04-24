// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_PAGE_ACTION_MODEL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/action_id.h"
#include "ui/menus/simple_menu_model.h"

namespace page_actions {

class MockPageActionModel : public PageActionModelInterface {
 public:
  MockPageActionModel();
  ~MockPageActionModel() override;

  MOCK_METHOD(actions::ActionId, GetActionId, (), (const, override));
  MOCK_METHOD(bool, GetVisible, (), (const, override));
  MOCK_METHOD(bool, IsChipShowing, (), (const, override));
  MOCK_METHOD(bool, ShouldShowSuggestionChip, (), (const, override));
  MOCK_METHOD(bool, GetShouldAnimateChipOut, (), (const, override));
  MOCK_METHOD(bool, GetShouldAnimateChipIn, (), (const, override));
  MOCK_METHOD(bool, GetShouldAnnounceChip, (), (const, override));
  MOCK_METHOD(bool, ShouldShowAnchoredMessage, (), (const, override));
  MOCK_METHOD(bool, IsAnchoredMessageShowing, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetText, (), (const, override));
  MOCK_METHOD(const std::u16string&, GetAccessibleName, (), (const, override));
  MOCK_METHOD(const std::u16string&,
              GetAnchoredMessageText,
              (),
              (const, override));
  MOCK_METHOD(AnchoredMessageActionIconType,
              GetAnchoredMessageActionIconType,
              (),
              (const, override));
  MOCK_METHOD(ui::SimpleMenuModel*,
              GetAnchoredMessageMenuModel,
              (),
              (const, override));
  MOCK_METHOD(const std::optional<ui::ImageModel>&,
              GetAnchoredMessageIcon,
              (),
              (const, override));
  MOCK_METHOD(const std::u16string&, GetTooltipText, (), (const, override));
  MOCK_METHOD(const ui::ImageModel&, GetImage, (), (const, override));
  MOCK_METHOD(bool, GetActionActive, (), (const, override));
  MOCK_METHOD(PageActionColorSource, GetColorSource, (), (const, override));
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
              (PageActionPassKey, const actions::ActionItem* action_item),
              (override));
  MOCK_METHOD(void,
              SetShowRequested,
              (PageActionPassKey, bool requested),
              (override));
  MOCK_METHOD(void,
              SetShouldShowSuggestionChip,
              (PageActionPassKey, bool show),
              (override));
  MOCK_METHOD(void,
              SetSuggestionChipConfig,
              (PageActionPassKey, const SuggestionChipConfig& config),
              (override));
  MOCK_METHOD(void,
              SetShouldShowAnchoredMessage,
              (PageActionPassKey, bool show),
              (override));
  MOCK_METHOD(void,
              SetAnchoredMessageText,
              (PageActionPassKey, const std::u16string& anchored_message),
              (override));
  MOCK_METHOD(void,
              SetAnchoredMessageAction,
              (PageActionPassKey,
               const AnchoredMessageActionIconType action_icon_type,
               std::unique_ptr<ui::SimpleMenuModel> model),
              (override));
  MOCK_METHOD(void,
              SetAnchoredMessageIcon,
              (PageActionPassKey, const std::optional<ui::ImageModel>& icon),
              (override));
  MOCK_METHOD(void,
              SetIsChipShowing,
              (PageActionPassKey, bool is_chip_showing),
              (override));
  MOCK_METHOD(void,
              SetIsAnchoredMessageShowing,
              (PageActionPassKey, bool is_anchored_message_showing),
              (override));
  MOCK_METHOD(void,
              SetHasPinnedIcon,
              (PageActionPassKey, bool has_pinned_icon),
              (override));
  MOCK_METHOD(void,
              SetTabActive,
              (PageActionPassKey, bool is_active),
              (override));
  MOCK_METHOD(void,
              SetOverrideText,
              (PageActionPassKey, const std::optional<std::u16string>& text),
              (override));
  MOCK_METHOD(void,
              SetOverrideAccessibleName,
              (PageActionPassKey,
               const std::optional<std::u16string>& override_accessible_name),
              (override));
  MOCK_METHOD(void,
              SetOverrideImage,
              (PageActionPassKey,
               const std::optional<ui::ImageModel>& override_text,
               PageActionColorSource color_source),
              (override));
  MOCK_METHOD(void,
              SetOverrideTooltip,
              (PageActionPassKey,
               const std::optional<std::u16string>& override_tooltip),
              (override));
  MOCK_METHOD(void,
              SetActionActive,
              (PageActionPassKey, bool is_active),
              (override));
  MOCK_METHOD(void,
              SetIsSuppressedByOmnibox,
              (PageActionPassKey, bool is_suppressed),
              (override));
  MOCK_METHOD(void,
              SetExemptFromOmniboxSuppression,
              (PageActionPassKey, bool is_exempt),
              (override));
  MOCK_METHOD(bool, IsEphemeral, (), (const, override));
};

template <typename PageActionModelType>
class FakePageActionModelFactory : public PageActionModelFactory {
 public:
  std::unique_ptr<PageActionModelInterface> Create(actions::ActionId action_id,
                                                   bool is_ephemeral) override {
    std::unique_ptr<PageActionModelType> model;
    if constexpr (std::is_constructible_v<PageActionModelType,
                                          actions::ActionId, bool>) {
      model = std::make_unique<PageActionModelType>(action_id, is_ephemeral);
    } else {
      model = std::make_unique<PageActionModelType>();
      ON_CALL(*model, GetActionId()).WillByDefault(testing::Return(action_id));
    }
    model_map_.emplace(action_id, model.get());
    return model;
  }

  // Model getter for tests to set expectations.
  PageActionModelType& Get(actions::ActionId action_id) {
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_MOCK_WEBUI_TOOLBAR_CONTROL_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_MOCK_WEBUI_TOOLBAR_CONTROL_DELEGATE_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/color/color_provider.h"

class BrowserWindowInterface;

namespace chrome {
class BrowserCommandController;
}  // namespace chrome

namespace views {
class View;
}  // namespace views

class MockWebUIToolbarControlDelegate
    : public WebUIToolbarControlDelegate,
      public webui_toolbar::IconTable::Delegate {
 public:
  MockWebUIToolbarControlDelegate();
  ~MockWebUIToolbarControlDelegate() override;

  MOCK_METHOD(BrowserWindowInterface*, GetBrowser, (), (override));
  MOCK_METHOD(chrome::BrowserCommandController*,
              GetCommandController,
              (),
              (override));
  MOCK_METHOD(views::View*, GetView, (), (override));
  MOCK_METHOD(void, AnnounceAlert, (const std::u16string&), (override));
  MOCK_METHOD(void, OnPreferredSizeChanged, (), (override));
  MOCK_METHOD(void,
              OnReloadControlStateChanged,
              (toolbar_ui_api::mojom::ReloadControlStatePtr state),
              (override));
  MOCK_METHOD(void,
              OnSplitTabsControlStateChanged,
              (toolbar_ui_api::mojom::SplitTabsControlStatePtr state),
              (override));
  MOCK_METHOD(void, OnBackForwardStateChanged, (), (override));
  MOCK_METHOD(void,
              OnHomeControlStateChanged,
              (toolbar_ui_api::mojom::HomeControlStatePtr state),
              (override));
  MOCK_METHOD(void,
              OnAppMenuControlStateChanged,
              (toolbar_ui_api::mojom::AppMenuControlStatePtr state),
              (override));
  MOCK_METHOD(void,
              OnOmniboxViewStateChanged,
              (toolbar_ui_api::mojom::OmniboxViewStatePtr state),
              (override));
  MOCK_METHOD(void,
              OnLocationBarFlagsChanged,
              (toolbar_ui_api::mojom::LocationBarFlagsPtr state),
              (override));
  MOCK_METHOD(void,
              OnSelectedKeywordChanged,
              (toolbar_ui_api::mojom::SelectedKeywordStatePtr state),
              (override));
  MOCK_METHOD(void,
              OnLhsChipsStateChanged,
              (toolbar_ui_api::mojom::LhsChipsStatePtr state),
              (override));
  MOCK_METHOD(
      void,
      OnPinnedToolbarActionsStateChanged,
      (std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> state),
      (override));
  MOCK_METHOD(
      void,
      OnContentSettingChanged,
      (std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state),
      (override));
  MOCK_METHOD(const toolbar_ui_api::mojom::NavigationControlsState&,
              GetState,
              (),
              (const override));
  MOCK_METHOD(void,
              OnAvatarControlStateChanged,
              (toolbar_ui_api::mojom::AvatarControlStatePtr),
              (override));
  MOCK_METHOD(void,
              OnFocusRequested,
              (toolbar_ui_api::mojom::FocusRequestTarget target),
              (override));
  webui_toolbar::IconTable& GetIconTable() override { return icon_table_; }

  // webui_toolbar::IconTable::Delegate:
  const ui::ColorProvider* GetColorProvider() const override;
  float GetScaleFactor() const override;

 private:
  webui_toolbar::IconTable icon_table_;
  ui::ColorProvider color_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_MOCK_WEBUI_TOOLBAR_CONTROL_DELEGATE_H_

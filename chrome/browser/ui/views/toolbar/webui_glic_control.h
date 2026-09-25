// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_CONTROL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button_interface.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/menus/simple_menu_model.h"

class Profile;
class WebUIToolbarControlDelegate;

namespace views {
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

class WebUIGlicControl : public glic::ToolbarGlicButtonInterface,
                         public ui::SimpleMenuModel::Delegate {
 public:
  explicit WebUIGlicControl(WebUIToolbarControlDelegate* delegate);
  WebUIGlicControl(const WebUIGlicControl&) = delete;
  WebUIGlicControl& operator=(const WebUIGlicControl&) = delete;
  ~WebUIGlicControl() override;

  void Init();
  void OnClicked();
  void HandleContextMenu(const gfx::Rect& screen_rect,
                         ui::mojom::MenuSourceType source);
  bool IsVisible() const { return should_show_ && is_pinned_; }
  bool IsContextMenuShowingForTesting() const;

  // glic::ToolbarGlicButtonInterface:
  void SetIsShowingNudge(bool is_showing) override;
  bool GetIsShowingNudge() const override;
  void SetNudgeLabel(std::string label) override;
  void SetVisible(bool visible) override;
  void SetGlicPanelIsOpen(bool open) override;
  void UpdateStyle(bool should_match_toolbar) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void OnMenuClosed();
  void OnPrefChanged();
  void UpdateState();

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;
  raw_ptr<Profile> profile_ = nullptr;
  PrefChangeRegistrar pref_registrar_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::string nudge_label_;
  bool is_initialized_ = false;
  bool is_panel_open_ = false;
  bool is_pinned_ = false;
  bool should_show_ = false;
  bool is_showing_nudge_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_CONTROL_H_

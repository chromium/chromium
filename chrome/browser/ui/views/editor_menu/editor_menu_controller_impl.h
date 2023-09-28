// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom-forward.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

// Implementation of ReadWriteCardController. It manages the editor menu related
// views.
class EditorMenuControllerImpl : public chromeos::ReadWriteCardController,
                                 public EditorMenuViewDelegate {
 public:
  EditorMenuControllerImpl();
  EditorMenuControllerImpl(const EditorMenuControllerImpl&) = delete;
  EditorMenuControllerImpl& operator=(const EditorMenuControllerImpl&) = delete;
  ~EditorMenuControllerImpl() override;

  // ReadWriteCardController:
  void OnContextMenuShown() override;
  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override;
  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override;
  void OnDismiss(bool is_other_command_executed) override;

  // EditorMenuViewDelegate:
  void OnSettingsButtonPressed() override;
  void OnChipButtonPressed(std::string_view text_query_id) override;
  void OnTextfieldArrowButtonPressed(std::u16string_view text) override;
  void OnPromoCardWidgetClosed(
      views::Widget::ClosedReason closed_reason) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;

  views::Widget* editor_menu_widget_for_testing() {
    return editor_menu_widget_.get();
  }

  void OnGetEditorPanelContextResultForTesting(
      const gfx::Rect& anchor_bounds,
      crosapi::mojom::EditorPanelContextPtr context);

 private:
  void OnGetEditorPanelContextResult(
      const gfx::Rect& anchor_bounds,
      crosapi::mojom::EditorPanelContextPtr context);

  views::UniqueWidgetPtr editor_menu_widget_;

  base::WeakPtrFactory<EditorMenuControllerImpl> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_

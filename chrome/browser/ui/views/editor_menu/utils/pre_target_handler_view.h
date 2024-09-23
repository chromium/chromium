// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos::editor_menu {

// A view that is attached with `chromeos::editor_menu::PreTargetHandler` and
// control the lifecycle of it.
class PreTargetHandlerView : public views::View,
                             public views::WidgetObserver,
                             public PreTargetHandler::Delegate {
  METADATA_HEADER(PreTargetHandlerView, views::View)

 public:
  explicit PreTargetHandlerView(const CardType& card_type = CardType::kDefault);

  PreTargetHandlerView(const PreTargetHandlerView&) = delete;
  PreTargetHandlerView& operator=(const PreTargetHandlerView&) = delete;

  ~PreTargetHandlerView() override;

  // views::View:
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // chromeos::editor_menu::PreTargetHandler::Delegate:
  views::View* GetRootView() override;
  std::vector<views::View*> GetTraversableViewsByUpDownKeys() override;

  void ResetPreTargetHandler();

 private:
  std::unique_ptr<chromeos::editor_menu::PreTargetHandler> pre_target_handler_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<PreTargetHandlerView> weak_ptr_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_VIEW_H_

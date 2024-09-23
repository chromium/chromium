// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

PreTargetHandlerView::PreTargetHandlerView(const CardType& card_type)
    : pre_target_handler_(
          std::make_unique<chromeos::editor_menu::PreTargetHandler>(
              /*delegate=*/*this,
              card_type)) {}

PreTargetHandlerView::~PreTargetHandlerView() = default;

void PreTargetHandlerView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
}

void PreTargetHandlerView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
}

void PreTargetHandlerView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  // When the widget is active, will use default focus behavior.
  if (active) {
    // Reset `pre_target_handler_` immediately causes problems if the events are
    // not all precessed. Reset it asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PreTargetHandlerView::ResetPreTargetHandler,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Close widget when it is deactivated.
  GetWidget()->Close();
}

views::View* PreTargetHandlerView::GetRootView() {
  return this;
}

std::vector<views::View*>
PreTargetHandlerView::GetTraversableViewsByUpDownKeys() {
  return {this};
}

void PreTargetHandlerView::ResetPreTargetHandler() {
  pre_target_handler_.reset();
}

BEGIN_METADATA(PreTargetHandlerView)
END_METADATA

}  // namespace chromeos::editor_menu

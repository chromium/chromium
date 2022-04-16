// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ReadAnythingCoordinator::ReadAnythingCoordinator(Browser* browser)
    : BrowserUserData<ReadAnythingCoordinator>(*browser) {
  // Create the model.
  model_ = std::make_unique<ReadAnythingModel>();

  // Create the controller.
  controller_ = std::make_unique<ReadAnythingController>(model_.get(), browser);

  // Create the views.
  auto toolbar = std::make_unique<ReadAnythingToolbarView>(
      controller_.get(), model_.get()->GetFontModel());

  auto content_web_view = std::make_unique<SidePanelWebUIViewT<ReadAnythingUI>>(
      browser,
      /* on_show_cb= */ base::RepeatingClosure(),
      /* close_cb= */ base::RepeatingClosure(),
      /* contents_wrapper= */
      std::make_unique<BubbleContentsWrapperT<ReadAnythingUI>>(
          /* webui_url= */ GURL(chrome::kChromeUIReadAnythingSidePanelURL),
          /* browser_context= */ browser->profile(),
          /* task_manager_string_id= */ IDS_READ_ANYTHING_TITLE,
          /* webui_resizes_host= */ false,
          /* esc_closes_ui= */ false));

  // Create the component.
  // Note that a coordinator would normally maintain ownership of these objects,
  // but objects extending {ui/views/view.h} prefer ownership over raw pointers.
  container_view_ = std::make_unique<ReadAnythingContainerView>(
      std::move(toolbar), std::move(content_web_view));
}

ReadAnythingCoordinator::~ReadAnythingCoordinator() = default;

void ReadAnythingCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      l10n_util::GetStringUTF16(IDS_READ_ANYTHING_TITLE),
      ui::ImageModel::FromVectorIcon(kReaderModeIcon, ui::kColorIcon),
      base::BindRepeating(&ReadAnythingCoordinator::GetContainerView,
                          base::Unretained(this))));
}

ReadAnythingPageHandler::Delegate*
ReadAnythingCoordinator::GetPageHandlerDelegate() {
  return controller_.get();
}

void ReadAnythingCoordinator::AddModelObserver(
    ReadAnythingModel::Observer* observer) {
  model_->AddObserver(observer);
}
void ReadAnythingCoordinator::RemoveModelObserver(
    ReadAnythingModel::Observer* observer) {
  model_->RemoveObserver(observer);
}

std::unique_ptr<views::View> ReadAnythingCoordinator::GetContainerView() {
  return std::move(container_view_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingCoordinator);

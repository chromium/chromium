// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"

#include <utility>

#include "mojo/public/cpp/bindings/clone_traits.h"

MockToolbarUIObserver::MockToolbarUIObserver() = default;
MockToolbarUIObserver::~MockToolbarUIObserver() = default;

mojo::PendingRemote<toolbar_ui_api::mojom::ToolbarUIObserver>
MockToolbarUIObserver::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockToolbarUIObserver::Bind(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIObserver> receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockToolbarUIObserver::FlushForTesting() {
  receiver_.FlushForTesting();
}

MockToolbarUIServiceDelegate::MockToolbarUIServiceDelegate() = default;
MockToolbarUIServiceDelegate::~MockToolbarUIServiceDelegate() = default;

MockBrowserControlsServiceDelegate::MockBrowserControlsServiceDelegate() =
    default;
MockBrowserControlsServiceDelegate::~MockBrowserControlsServiceDelegate() =
    default;

toolbar_ui_api::mojom::NavigationControlsStatePtr
CreateValidNavigationControlsState() {
  auto back_forward_state =
      toolbar_ui_api::mojom::BackForwardControlState::New();
  back_forward_state->back_button_state =
      toolbar_ui_api::mojom::BackForwardButtonState::New();
  back_forward_state->forward_button_state =
      toolbar_ui_api::mojom::BackForwardButtonState::New();
  return toolbar_ui_api::mojom::NavigationControlsState::New(
      toolbar_ui_api::mojom::ReloadControlState::New(),
      toolbar_ui_api::mojom::SplitTabsControlState::New(),
      std::move(back_forward_state),
      toolbar_ui_api::mojom::HomeControlState::New(),
      toolbar_ui_api::mojom::LocationBarState::New(
          toolbar_ui_api::mojom::OmniboxViewState::New(),
          toolbar_ui_api::mojom::LocationBarFlags::New(),
          std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
          toolbar_ui_api::mojom::LhsChipsState::New(
              toolbar_ui_api::mojom::SecurityChipState::New(
                  toolbar_ui_api::mojom::SecurityChipIcon::kHttp,
                  toolbar_ui_api::mojom::SecurityLevel::kNone, std::u16string(),
                  /*is_clickable=*/false, /*is_text_dangerous=*/false,
                  /*is_visible=*/true),
              std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
              /*permission_dashboard=*/nullptr)),
      std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>(),
      /*layout_constants_version=*/0);
}

MockCommandUpdater::MockCommandUpdater() = default;
MockCommandUpdater::~MockCommandUpdater() = default;

namespace mojo {
std::ostream& operator<<(
    std::ostream& out,
    const toolbar_ui_api::mojom::IconUpdatePtr& icon_update) {
  return out << "{handle_id: " << icon_update->handle_id
             << ", icon_url_or_name: "
             << icon_update->icon_url_or_name.value_or(std::string("(nullopt)"))
             << ", icon_is_url: " << icon_update->icon_is_url << "}";
}

}  // namespace mojo

FakeIconTableFetcher::FakeIconTableFetcher() = default;
FakeIconTableFetcher::~FakeIconTableFetcher() = default;

void FakeIconTableFetcher::AddUpdate(
    toolbar_ui_api::mojom::IconUpdatePtr update) {
  all_updates_.push_back(update.Clone());
  pending_updates_.push_back(std::move(update));
}

std::vector<toolbar_ui_api::mojom::IconUpdatePtr>
FakeIconTableFetcher::GetFullState() {
  return mojo::Clone(all_updates_);
}

std::vector<toolbar_ui_api::mojom::IconUpdatePtr>
FakeIconTableFetcher::TakePendingUpdates() {
  return std::move(pending_updates_);
}

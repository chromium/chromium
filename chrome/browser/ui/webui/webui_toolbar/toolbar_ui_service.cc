// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace {
using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToNotLoading.Start";
}  // namespace

namespace toolbar_ui_api {

ToolbarUIService::ToolbarUIService(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> service,
    std::unique_ptr<NavigationControlsStateFetcher> state_fetcher,
    std::unique_ptr<IconTableFetcher> icon_table_fetcher,
    MetricsReporter* metrics_reporter,
    ToolbarUIServiceDelegate* delegate)
    : service_(this, std::move(service)),
      state_fetcher_(std::move(state_fetcher)),
      icon_table_fetcher_(std::move(icon_table_fetcher)),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate) {
  CHECK(state_fetcher_);
}

ToolbarUIService::~ToolbarUIService() = default;

void ToolbarUIService::SetDelegate(ToolbarUIServiceDelegate* delegate) {
  delegate_ = delegate;
}

void ToolbarUIService::OnNavigationControlsStateChanged(
    const mojom::NavigationControlsState& state) {
  auto* mark = state.reload_control_state->is_navigation_loading
                   ? kChangeVisibleModeToLoadingStartMark
                   : kChangeVisibleModeToNotLoadingStartMark;
  metrics_reporter_->Mark(mark);

  // We want to call this even if no one is listening since they'll just get
  // all the info as the initial state.
  auto icon_updates = icon_table_fetcher_->TakePendingUpdates();

  if (observers_.empty()) {
    return;
  }

  auto it = observers_.begin();
  while (true) {
    const auto& observer = *it;
    ++it;
    if (it == observers_.end()) {
      // Last item, can avoid some copies.
      observer->OnNavigationControlsStateChanged(std::move(icon_updates),
                                                 state.Clone());
      break;
    } else {
      observer->OnNavigationControlsStateChanged(mojo::Clone(icon_updates),
                                                 state.Clone());
    }
  }
}

void ToolbarUIService::Bind(BindCallback callback) {
  auto result = toolbar_ui_api::mojom::InitialState::New();
  result->state = state_fetcher_->GetNavigationControlsState();
  result->icons = icon_table_fetcher_->GetFullState();

  mojo::Remote<toolbar_ui_api::mojom::ToolbarUIObserver> observer;
  result->update_stream = observer.BindNewPipeAndPassReceiver();

  observers_.Add(std::move(observer));

  std::move(callback).Run(std::move(result));
}

void ToolbarUIService::OnFocusRequested(mojom::FocusRequestTarget target) {
  for (const auto& observer : observers_) {
    observer->OnFocusRequested(target);
  }
}

void ToolbarUIService::ShowContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    const gfx::RectF& bounds_in_css_pixels,
    ui::mojom::MenuSourceType source) {
  if (delegate_) {
    delegate_->HandleContextMenu(menu_type, bounds_in_css_pixels, source);
  }
}

void ToolbarUIService::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action,
    OnOmniboxActionCallback callback) {
  if (delegate_) {
    std::move(callback).Run(delegate_->OnOmniboxAction(std::move(action)));
  } else {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kFailedPrecondition,
                   "ToolbarUIService: null delegate_ for OnOmniboxAction")));
  }
}

void ToolbarUIService::OnPageInitialized() {
  if (delegate_) {
    delegate_->OnPageInitialized();
  }
}

void ToolbarUIService::ShowContentSettingsBubble(
    ::toolbar_ui_api::mojom::ContentSettingImageType type,
    ShowContentSettingsBubbleCallback callback) {
  if (delegate_) {
    delegate_->ShowContentSettingsBubble(type, std::move(callback));
  } else {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kFailedPrecondition,
                   base::StringPrintf("ToolbarUIService: cannot create bubble "
                                      "without delegate_ for type: %d",
                                      static_cast<int32_t>(type)))));
  }
}

void ToolbarUIService::OnPageActionClick(
    ::toolbar_ui_api::mojom::PageActionId action_id,
    ::toolbar_ui_api::mojom::PageActionTrigger trigger,
    OnPageActionClickCallback callback) {
  if (delegate_) {
    delegate_->OnPageActionClick(action_id, trigger, std::move(callback));
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        base::StringPrintf("ToolbarUIService: cannot click page action "
                           "(action_id=%d, trigger=%d) without delegate_",
                           static_cast<int>(action_id),
                           static_cast<int>(trigger)))));
  }
}

void ToolbarUIService::OnPageActionChipShowingChanged(
    ::toolbar_ui_api::mojom::PageActionId action_id,
    OnPageActionChipShowingChangedCallback callback) {
  if (delegate_) {
    delegate_->OnPageActionChipShowingChanged(action_id, std::move(callback));
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        base::StringPrintf("ToolbarUIService: cannot change page action "
                           "chip showing (action_id=%d) without delegate_",
                           static_cast<int>(action_id)))));
  }
}

void ToolbarUIService::InvokePinnedToolbarAction(
    toolbar_ui_api::mojom::PinnedToolbarAction action_id) {
  if (delegate_) {
    delegate_->InvokePinnedToolbarAction(action_id);
  }
}

void ToolbarUIService::OnLocationBarFocusWithinChanged(bool focused) {
  if (delegate_) {
    delegate_->OnLocationBarFocusWithinChanged(focused);
  }
}

void ToolbarUIService::OnLhsChipMousePressed(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (delegate_) {
    delegate_->OnLhsChipMousePressed(identifier);
  }
}

void ToolbarUIService::OnLhsChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction) {
  if (delegate_) {
    delegate_->OnLhsChipClicked(identifier, is_mouse_interaction);
  }
}

void ToolbarUIService::OnLhsChipPointerEntered(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (delegate_) {
    delegate_->OnLhsChipPointerEntered(identifier);
  }
}

void ToolbarUIService::OnLhsChipPointerExited(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (delegate_) {
    delegate_->OnLhsChipPointerExited(identifier);
  }
}

void ToolbarUIService::OnLhsChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (delegate_) {
    delegate_->OnLhsChipExpandAnimationEnded(identifier);
  }
}

void ToolbarUIService::OnLhsChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (delegate_) {
    delegate_->OnLhsChipCollapseAnimationEnded(identifier);
  }
}

void ToolbarUIService::OnLhsChipDrag(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    ui::mojom::DragEventSource source) {
  if (delegate_) {
    delegate_->OnLhsChipDrag(identifier, source);
  }
}

void ToolbarUIService::OnHomeButtonDropUrl(const GURL& url) {
  if (delegate_) {
    delegate_->OnHomeButtonDropUrl(url);
  }
}

void ToolbarUIService::OnHomeButtonDropFile(const gfx::PointF& drop_position) {
  if (delegate_) {
    delegate_->OnHomeButtonDropFile(drop_position);
  }
}

void ToolbarUIService::OnToolbarDropFile(const gfx::PointF& drop_position) {
  if (delegate_) {
    delegate_->OnToolbarDropFile(drop_position);
  }
}

void ToolbarUIService::ShowAvatarMenu(ShowAvatarMenuCallback callback) {
  if (delegate_) {
    delegate_->ShowAvatarMenu();
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        "ToolbarUIService: cannot show avatar menu without delegate_")));
  }
}

void ToolbarUIService::SetAvatarButtonHovered(
    bool hovered,
    SetAvatarButtonHoveredCallback callback) {
  if (delegate_) {
    delegate_->SetAvatarButtonHovered(hovered);
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        "ToolbarUIService: cannot hover on avatar without delegate_")));
  }
}

void ToolbarUIService::SetAvatarButtonFocused(
    bool focused,
    SetAvatarButtonFocusedCallback callback) {
  if (delegate_) {
    delegate_->SetAvatarButtonFocused(focused);
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        "ToolbarUIService: cannot focus on avatar without delegate_")));
  }
}

void ToolbarUIService::SetAvatarButtonIphPromoShowing(
    bool showing,
    SetAvatarButtonIphPromoShowingCallback callback) {
  if (delegate_) {
    delegate_->SetAvatarButtonIPHPromoShowing(showing);
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(
        base::unexpected(Error::New(Code::kFailedPrecondition,
                                    "ToolbarUIService: cannot set IPH promo "
                                    "showing on avatar without delegate_")));
  }
}

void ToolbarUIService::OnAppMenuFocusChanged(bool focused) {
  if (delegate_) {
    delegate_->OnAppMenuFocusChanged(focused);
  }
}

void ToolbarUIService::ExecuteExtensionAction(const std::string& extension_id) {
  if (delegate_) {
    delegate_->ExecuteExtensionAction(extension_id);
  }
}

void ToolbarUIService::ShowExtensionContextMenu(
    const std::string& extension_id,
    ui::mojom::MenuSourceType source) {
  if (delegate_) {
    delegate_->ShowExtensionContextMenu(extension_id, source);
  }
}
}  // namespace toolbar_ui_api

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "url/gurl.h"

namespace tabs_api {

// Starts a session that suppresses incoming messages to prevent re-entrancy and
// replays all recorded mutations on session destruction.
class ApiSession : public TabStripServiceImpl::Session {
 public:
  using OnApiSessionEndCb = base::OnceCallback<void()>;

  explicit ApiSession(OnApiSessionEndCb cb)
      : on_session_ended_(std::move(cb)) {}

  ~ApiSession() override { std::move(on_session_ended_).Run(); }

  // Disallow copy and assign.
  ApiSession(const ApiSession&) = delete;
  ApiSession& operator=(const ApiSession&) = delete;

 private:
  base::OnceCallback<void()> on_session_ended_;
};

class SessionControllerImpl : public TabStripServiceImpl::SessionController {
 public:
  explicit SessionControllerImpl(
      tabs_api::events::TabStripEventRecorder* recorder)
      : recorder_(recorder) {}

  ~SessionControllerImpl() override = default;

  std::unique_ptr<TabStripServiceImpl::Session> CreateSession() override {
    CHECK(!session_in_progress_)
        << "re-entrancy into tab strip service is not allowed";

    recorder_->StopNotificationAndStartRecording();
    auto session = std::make_unique<ApiSession>(base::BindOnce(
        &SessionControllerImpl::EndSession, base::Unretained(this)));
    session_in_progress_ = true;

    return std::move(session);
  }

  void EndSession() {
    recorder_->PlayRecordingsAndStartNotification();
    session_in_progress_ = false;
  }

 private:
  bool session_in_progress_ = false;
  raw_ptr<tabs_api::events::TabStripEventRecorder> recorder_;
};

TabStripServiceImpl::TabStripServiceImpl(BrowserWindowInterface* browser,
                                         TabStripModel* tab_strip_model)
    : TabStripServiceImpl(
          std::make_unique<tabs_api::BrowserAdapterImpl>(browser),
          std::make_unique<tabs_api::TabStripModelAdapterImpl>(
              tab_strip_model)) {}

TabStripServiceImpl::TabStripServiceImpl(
    std::unique_ptr<BrowserAdapter> browser_adapter,
    std::unique_ptr<TabStripModelAdapter> tab_strip_model_adapter)
    : browser_adapter_(std::move(browser_adapter)),
      tab_strip_model_adapter_(std::move(tab_strip_model_adapter)) {
  recorder_ = std::make_unique<tabs_api::events::TabStripEventRecorder>(
      tab_strip_model_adapter_.get(),
      base::BindRepeating(&TabStripServiceImpl::BroadcastEvents,
                          base::Unretained(this)));
  session_controller_ =
      std::make_unique<SessionControllerImpl>(recorder_.get());

  tab_strip_model_adapter_->AddModelObserver(recorder_.get());
  tab_strip_model_adapter_->AddCollectionObserver(recorder_.get());
}

TabStripServiceImpl::~TabStripServiceImpl() {
  tab_strip_model_adapter_->RemoveModelObserver(recorder_.get());
  tab_strip_model_adapter_->RemoveCollectionObserver(recorder_.get());
}

void TabStripServiceImpl::BroadcastEvents(
    const std::vector<events::Event>& events) const {
  tabs_api::EventBroadcaster broadcaster;
  broadcaster.Broadcast(observers_, events);
}

TabStripService::GetTabsResult TabStripServiceImpl::GetTabs() {
  auto session = session_controller_->CreateSession();

  return tab_strip_model_adapter_->GetTabStripTopology(
      tab_strip_model_adapter_->GetRoot()->GetHandle());
}

mojom::TabStripService::GetTabResult TabStripServiceImpl::GetTab(
    const tabs_api::NodeId& tab_mojom_id) {
  auto session = session_controller_->CreateSession();

  if (tab_mojom_id.Type() != tabs_api::NodeId::Type::kContent) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "only tab content ids accepted"));
  }

  int32_t tab_id;
  if (!base::StringToInt(tab_mojom_id.Id(), &tab_id)) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tab id provided"));
  }

  tabs_api::mojom::TabPtr tab_result;
  // TODO (crbug.com/412709270) TabStripModel or TabCollections should have an
  // api that can fetch id without of relying  on indices.
  auto tabs = tab_strip_model_adapter_->GetTabs();
  for (unsigned int i = 0; i < tabs.size(); ++i) {
    auto& handle = tabs.at(i);
    if (tab_id == handle.raw_value()) {
      auto renderer_data = tab_strip_model_adapter_->GetTabRendererData(i);
      const ui::ColorProvider& color_provider =
          tab_strip_model_adapter_->GetColorProvider();
      tab_result = tabs_api::converters::BuildMojoTab(
          handle, renderer_data, color_provider,
          tab_strip_model_adapter_->GetTabStates(handle));
    }
  }

  if (tab_result) {
    return std::move(tab_result);
  } else {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "Tab not found"));
  }
}

mojom::TabStripService::CreateTabAtResult TabStripServiceImpl::CreateTabAt(
    const std::optional<tabs_api::Position>& pos,
    const std::optional<GURL>& url) {
  auto session = session_controller_->CreateSession();

  GURL target_url;
  if (url.has_value()) {
    target_url = url.value();
  }
  tabs_api::InsertionParams params =
      tab_strip_model_adapter_->CalculateInsertionParams(pos);
  auto tab_handle = browser_adapter_->AddTabAt(target_url, params.index,
                                               params.group_id, params.pinned);
  if (tab_handle == tabs::TabHandle::Null()) {
    // Missing content can happen for a number of reasons. i.e. If the profile
    // is shutting down or if navigation requests are blocked due to some
    // internal state. This is usually because the browser is not in the
    // required state to perform the action.
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInternal, "Failed to create WebContents"));
  }

  auto tab_index = tab_strip_model_adapter_->GetIndexForHandle(tab_handle);
  if (!tab_index.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInternal,
        "Could not find the index of the newly created tab"));
  }

  auto renderer_data =
      tab_strip_model_adapter_->GetTabRendererData(tab_index.value());
  const ui::ColorProvider& color_provider =
      tab_strip_model_adapter_->GetColorProvider();
  auto mojo_tab = tabs_api::converters::BuildMojoTab(
      tab_handle, renderer_data, color_provider,
      tab_strip_model_adapter_->GetTabStates(tab_handle));
  return mojo_tab->Clone();
}

mojom::TabStripService::CloseTabsResult TabStripServiceImpl::CloseTabs(
    const std::vector<tabs_api::NodeId>& ids) {
  auto session = session_controller_->CreateSession();

  std::vector<int32_t> tab_content_targets;
  for (const auto& id : ids) {
    if (id.Type() != tabs_api::NodeId::Type::kContent) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kUnimplemented,
          "only content tab closing has been implemented right now"));
    }
    int32_t numeric_id;
    if (!base::StringToInt(id.Id(), &numeric_id)) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "invalid tab content id"));
    }
    tab_content_targets.push_back(numeric_id);
  }

  std::vector<size_t> tab_strip_indices;
  // Transform targets from ids to indices in the tabstrip.
  for (auto target : tab_content_targets) {
    auto target_idx =
        tab_strip_model_adapter_->GetIndexForHandle(tabs::TabHandle(target));
    if (!target_idx.has_value()) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kNotFound, "could not find the a tab"));
    }
    tab_strip_indices.push_back(target_idx.value());
  }

  // Close from last to first, that way the removals won't change the index of
  // the next target.
  std::ranges::sort(tab_strip_indices, std::ranges::greater());
  for (auto idx : tab_strip_indices) {
    tab_strip_model_adapter_->CloseTab(idx);
  }

  return std::monostate();
}

mojom::TabStripService::ActivateTabResult TabStripServiceImpl::ActivateTab(
    const tabs_api::NodeId& id) {
  auto session = session_controller_->CreateSession();

  if (id.Type() != tabs_api::NodeId::Type::kContent) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "only a content tab id can be provided"));
  }

  int32_t handle_id;
  if (!base::StringToInt(id.Id(), &handle_id)) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "id is malformed"));
  }

  auto maybe_idx =
      tab_strip_model_adapter_->GetIndexForHandle(tabs::TabHandle(handle_id));
  if (!maybe_idx.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "tab not found"));
  }

  tab_strip_model_adapter_->ActivateTab(maybe_idx.value());
  return std::monostate();
}

mojom::TabStripService::SetSelectedTabsResult
TabStripServiceImpl::SetSelectedTabs(
    const std::vector<tabs_api::NodeId>& selection,
    const tabs_api::NodeId& tab_to_activate) {
  auto session = session_controller_->CreateSession();

  if (std::find(selection.begin(), selection.end(), tab_to_activate) ==
      selection.end()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument,
        "the selection must include the tab_to_activate"));
  }

  auto is_not_content_id = [](tabs_api::NodeId id) {
    return id.Type() != tabs_api::NodeId::Type::kContent;
  };
  if (std::find_if(selection.begin(), selection.end(), is_not_content_id) !=
      selection.end()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument,
        "the selection can only include content IDs"));
  }

  std::vector<tabs::TabHandle> selection_handles;
  for (auto& id : selection) {
    int32_t handle_id;
    if (!base::StringToInt(id.Id(), &handle_id)) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "id is malformed"));
    }
    selection_handles.push_back(tabs::TabHandle(handle_id));
  }

  int32_t activate_handle_id;
  if (!base::StringToInt(tab_to_activate.Id(), &activate_handle_id)) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "activate id is malformed"));
  }

  tab_strip_model_adapter_->SetTabSelection(
      selection_handles, tabs::TabHandle(activate_handle_id));

  return std::monostate();
}

mojom::TabStripService::MoveNodeResult TabStripServiceImpl::MoveNode(
    const tabs_api::NodeId& id,
    const tabs_api::Position& position) {
  auto session = session_controller_->CreateSession();

  if (position.index() >= tab_strip_model_adapter_->GetTabs().size()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "position cannot exceed tab strip"));
  }

  switch (id.Type()) {
    case tabs_api::NodeId::Type::kContent: {
      std::optional<tabs::TabHandle> tab_handle = id.ToTabHandle();
      if (!tab_handle.has_value()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "id is malformed"));
      }
      // TODO(crbug.com/409086859): Add error handling for cases where a
      // position's parent id is impossible to be moved to.
      tab_strip_model_adapter_->MoveTab(tab_handle.value(), position);
      break;
    }
    case tabs_api::NodeId::Type::kCollection: {
      tab_strip_model_adapter_->MoveCollection(id, position);
      break;
    }
    default:
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "invalid node type"));
  }

  return std::monostate();
}

// tabs_api::mojom::TabStripExperimentalService overrides
//
// TabStripExperimentalService is intended for quick prototyping for
// experimental apis that may not necessarily fit in the standard
// TabStripService.
mojom::TabStripExperimentService::UpdateTabGroupVisualResult
TabStripServiceImpl::UpdateTabGroupVisual(
    const tabs_api::NodeId& id,
    const tab_groups::TabGroupVisualData& visual_data) {
  auto session = session_controller_->CreateSession();

  if (id.Type() != tabs_api::NodeId::Type::kCollection) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "id must be a collection"));
  }

  const std::optional<tabs::TabCollectionHandle> collection_handle =
      id.ToTabCollectionHandle();
  if (!collection_handle.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "id is malformed"));
  }

  const std::optional<const tab_groups::TabGroupId> group_id =
      tab_strip_model_adapter_->FindGroupIdFor(collection_handle.value());
  if (!group_id.has_value()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kNotFound,
                                     "group with the specified ID not found."));
  }

  tab_strip_model_adapter_->UpdateTabGroupVisuals(group_id.value(),
                                                  visual_data);

  return std::monostate();
}

void TabStripServiceImpl::AddObserver(
    observation::TabStripApiBatchedObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripServiceImpl::RemoveObserver(
    observation::TabStripApiBatchedObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace tabs_api

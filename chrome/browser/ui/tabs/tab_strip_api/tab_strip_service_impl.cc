// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/experimental_platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/platform_adapters_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_id_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
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

TabStripServiceImpl::TabStripServiceImpl(
    std::unique_ptr<PlatformAdaptersProvider> adapters_provider,
    std::unique_ptr<ExperimentalPlatformAdaptersProvider>
        experimental_adapters_provider)
    : adapters_provider_(std::move(adapters_provider)),
      experimental_adapters_provider_(
          std::move(experimental_adapters_provider)) {
  recorder_ = std::make_unique<tabs_api::events::TabStripEventRecorder>(
      base::BindRepeating(&TabStripServiceImpl::BroadcastEvents,
                          base::Unretained(this)));
  session_controller_ =
      std::make_unique<SessionControllerImpl>(recorder_.get());

  adapters_provider_->event_bridge().AddObserver(recorder_.get());
  AddObserver(this);
}

TabStripServiceImpl::~TabStripServiceImpl() {
  RemoveObserver(this);
  adapters_provider_->event_bridge().RemoveObserver(recorder_.get());
}

TabStripModelAdapter& TabStripServiceImpl::tab_strip_model_adapter() {
  return adapters_provider_->tab_strip_model_adapter();
}

TranslationAdapter& TabStripServiceImpl::translation_adapter() {
  return adapters_provider_->translation_adapter();
}

BrowserAdapter& TabStripServiceImpl::browser_adapter() {
  return adapters_provider_->browser_adapter();
}

ContextMenuAdapter* TabStripServiceImpl::context_menu_adapter() {
  return experimental_adapters_provider_
             ? &experimental_adapters_provider_->context_menu_adapter()
             : nullptr;
}

void TabStripServiceImpl::BroadcastEvents(
    const std::vector<events::Event>& events) const {
  tabs_api::EventBroadcaster broadcaster;
  broadcaster.Broadcast(observers_, events);
}

TabStripService::GetTabsResult
TabStripServiceImpl::GetTabsWithoutObservation() {
  auto session = session_controller_->CreateSession();

  return tab_strip_model_adapter().GetTabStripTopology(
      tab_strip_model_adapter().GetRoot()->GetHandle());
}

mojom::TabStripService::GetTabsResult TabStripServiceImpl::GetTabs() {
  auto snapshot = tabs_api::mojom::TabsSnapshot::New();
  ASSIGN_OR_RETURN(auto result, GetTabsWithoutObservation());
  snapshot->tab_strip = std::move(result);

  mojo::AssociatedRemote<tabs_api::mojom::TabsObserver> stream;
  auto pending_receiver = stream.BindNewEndpointAndPassReceiver();
  mojo_observers_.Add(std::move(stream));
  snapshot->stream = std::move(pending_receiver);

  return snapshot;
}

mojom::TabStripService::GetTabResult TabStripServiceImpl::GetTab(
    const tabs_api::NodeId& tab_mojom_id) {
  auto session = session_controller_->CreateSession();

  ASSIGN_OR_RETURN(auto tab_id, utils::GetContentNativeTabId(tab_mojom_id));

  tabs_api::mojom::TabPtr tab_result;
  // TODO (crbug.com/412709270) TabStripModel or TabCollections should have an
  // api that can fetch id without of relying  on indices.
  auto tabs = tab_strip_model_adapter().GetTabs();
  for (unsigned int i = 0; i < tabs.size(); ++i) {
    auto& handle = tabs.at(i);
    if (tab_id == handle.raw_value()) {
      ASSIGN_OR_RETURN(tab_result, translation_adapter().ToMojoTab(handle));
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

  if (pos.has_value()) {
    RETURN_IF_ERROR(utils::CheckPath(
        pos->path(),
        NodeId::FromWindowId(tab_strip_model_adapter().GetWindowId()),
        NodeId::FromTabCollectionHandle(
            tab_strip_model_adapter().GetRoot()->GetHandle())));
  }

  GURL target_url;
  if (url.has_value()) {
    target_url = url.value();
  }
  tabs_api::InsertionParams params =
      tab_strip_model_adapter().CalculateInsertionParams(pos);
  auto tab_handle = browser_adapter().AddTabAt(target_url, params.index,
                                               params.group_id, params.pinned);
  if (tab_handle == tabs::TabHandle::Null()) {
    // Missing content can happen for a number of reasons. i.e. If the profile
    // is shutting down or if navigation requests are blocked due to some
    // internal state. This is usually because the browser is not in the
    // required state to perform the action.
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInternal, "Failed to create WebContents"));
  }

  auto tab_index = tab_strip_model_adapter().GetIndexForHandle(tab_handle);
  if (!tab_index.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInternal,
        "Could not find the index of the newly created tab"));
  }

  ASSIGN_OR_RETURN(auto mojo_tab, translation_adapter().ToMojoTab(tab_handle));
  return std::move(mojo_tab);
}

mojom::TabStripService::CloseNodesResult TabStripServiceImpl::CloseNodes(
    const std::vector<tabs_api::NodeId>& ids) {
  auto session = session_controller_->CreateSession();

  std::vector<tabs::TabHandle> tab_targets;
  for (const auto& id : ids) {
    if (id.Type() == NodeId::Type::kCollection) {
      RETURN_IF_ERROR(CloseCollection(id));
    } else {
      ASSIGN_OR_RETURN(auto handle_id, utils::GetContentNativeTabId(id));
      tab_targets.emplace_back(handle_id);
    }
  }

  CloseTabs(tab_targets);

  return std::monostate();
}

base::expected<void, mojo_base::mojom::ErrorPtr>
TabStripServiceImpl::CloseCollection(const NodeId& id) {
  ASSIGN_OR_RETURN(auto group_id,
                   utils::GetTabGroupId(tab_strip_model_adapter(), id));
  tab_strip_model_adapter().CloseTabGroup(group_id);

  return base::ok();
}

void TabStripServiceImpl::CloseTabs(
    const std::vector<tabs::TabHandle>& tab_targets) {
  std::vector<size_t> tab_strip_indices;
  // Transform targets from handles to indices in the tabstrip. Handles that
  // are no longer in the strip (because they were part of a closed group) are
  // ignored.
  for (auto handle : tab_targets) {
    auto target_idx = tab_strip_model_adapter().GetIndexForHandle(handle);
    if (target_idx.has_value()) {
      tab_strip_indices.push_back(target_idx.value());
    }
  }

  // Close from last to first, that way the removals won't change the index of
  // the next target.
  std::ranges::sort(tab_strip_indices, std::ranges::greater());
  for (auto idx : tab_strip_indices) {
    tab_strip_model_adapter().CloseTab(idx);
  }
}

mojom::TabStripService::ActivateTabResult TabStripServiceImpl::ActivateTab(
    const tabs_api::NodeId& id) {
  auto session = session_controller_->CreateSession();

  ASSIGN_OR_RETURN(auto tab_id, utils::GetContentNativeTabId(id));

  auto maybe_idx =
      tab_strip_model_adapter().GetIndexForHandle(tabs::TabHandle(tab_id));
  if (!maybe_idx.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "tab not found"));
  }

  tab_strip_model_adapter().ActivateTab(maybe_idx.value());
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
    ASSIGN_OR_RETURN(auto handle_id, utils::GetNativeId(id));
    selection_handles.emplace_back(handle_id);
  }

  ASSIGN_OR_RETURN(auto activate_handle_id,
                   utils::GetNativeId(tab_to_activate));

  tab_strip_model_adapter().SetTabSelection(
      selection_handles, tabs::TabHandle(activate_handle_id));

  return std::monostate();
}

mojom::TabStripService::MoveNodeResult TabStripServiceImpl::MoveNode(
    const tabs_api::NodeId& id,
    const tabs_api::Position& position) {
  auto session = session_controller_->CreateSession();

  RETURN_IF_ERROR(utils::CheckPath(
      position.path(),
      NodeId::FromWindowId(tab_strip_model_adapter().GetWindowId()),
      NodeId::FromTabCollectionHandle(
          tab_strip_model_adapter().GetRoot()->GetHandle())));

  if (position.index() >= tab_strip_model_adapter().GetTabs().size()) {
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
      tab_strip_model_adapter().MoveTab(tab_handle.value(), position);
      break;
    }
    case tabs_api::NodeId::Type::kCollection: {
      tab_strip_model_adapter().MoveCollection(id, position);
      break;
    }
    default:
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "invalid node type"));
  }

  return std::monostate();
}

mojom::TabStripService::UpdateResult TabStripServiceImpl::Update(
    mojom::DataPtr data,
    const std::optional<std::vector<std::string>>& update_mask) {
  auto session = session_controller_->CreateSession();

  switch (data->which()) {
    case mojom::Data::Tag::kTabGroup:
      return UpdateTabGroup(std::move(data->get_tab_group()), update_mask);
    default:
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kUnimplemented,
          "Update not implemented for resource type: " +
              base::ToString(data->which())));
  }
}

mojom::TabStripService::UpdateResult TabStripServiceImpl::UpdateTabGroup(
    mojom::TabGroupPtr tab_group,
    const std::optional<std::vector<std::string>>& update_mask) {
  ASSIGN_OR_RETURN(
      auto group_id,
      utils::GetTabGroupId(tab_strip_model_adapter(), tab_group->id));

  auto collection_handle =
      tab_strip_model_adapter().GetCollectionHandleForTabGroupId(group_id);
  ASSIGN_OR_RETURN(auto current_data,
                   translation_adapter().ToMojoData(collection_handle));

  ASSIGN_OR_RETURN(
      auto updated_visual_data,
      utils::MergeTabGroupVisualData(current_data->get_tab_group()->data,
                                     tab_group->data, update_mask));

  tab_strip_model_adapter().UpdateTabGroupVisuals(group_id,
                                                  updated_visual_data);
  return translation_adapter().ToMojoData(collection_handle);
}

// tabs_api::mojom::TabStripExperimentalService overrides
//
// TabStripExperimentalService is intended for quick prototyping for
// experimental apis that may not necessarily fit in the standard
// TabStripService.
mojom::TabStripExperimentService::ShowTabContextMenuResult
TabStripServiceImpl::ShowTabContextMenu(const tabs_api::NodeId& tab_id,
                                        const gfx::Point& location) {
  auto session = session_controller_->CreateSession();

  std::optional<tabs::TabHandle> tab_handle = tab_id.ToTabHandle();
  if (!tab_handle.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tab id"));
  }

  ContextMenuAdapter* adapter = context_menu_adapter();
  if (!adapter) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kUnimplemented, "Context menu not supported"));
  }

  RETURN_IF_ERROR(adapter->ShowTabContextMenu(tab_handle.value(), location));

  return std::monostate();
}

mojom::TabStripExperimentService::GetAllTabsForProfileResult
TabStripServiceImpl::GetAllTabsForProfile() {
  auto session = session_controller_->CreateSession();
  base::flat_map<std::string, mojom::ContainerPtr> windows;
  for (auto& adapter :
       browser_adapter().CreateAllTabStripModelAdaptersForProfile()) {
    windows.emplace(
        adapter->GetWindowId(),
        adapter->GetTabStripTopology(adapter->GetRoot()->GetHandle()));
  }

  return windows;
}

void TabStripServiceImpl::AddObserver(
    observation::TabStripApiBatchedObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripServiceImpl::RemoveObserver(
    observation::TabStripApiBatchedObserver* observer) {
  observers_.RemoveObserver(observer);
}

// TODO(crbug.com/445765534): we should probably just move the mojo bits out
// of this class into their own object. Interleaving them in this class leads
// to some pertty strange code...
void TabStripServiceImpl::OnTabEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  for (auto& observer : mojo_observers_) {
    std::vector<tabs_api::mojom::TabsEventPtr> copy;
    for (auto& event : events) {
      copy.push_back(event.Clone());
    }
    observer->OnTabEvents(std::move(copy));
  }
}

mojom::TabStripExperimentService::ReplaceTabInSplitResult
TabStripServiceImpl::ReplaceTabInSplit(const tabs_api::NodeId& tab_to_replace,
                                       const tabs_api::NodeId& tab_to_insert) {
  auto session = session_controller_->CreateSession();

  ASSIGN_OR_RETURN(auto replace_handle_id,
                   utils::GetContentNativeTabId(tab_to_replace));
  ASSIGN_OR_RETURN(auto insert_handle_id,
                   utils::GetContentNativeTabId(tab_to_insert));

  tabs::TabHandle replace_handle(replace_handle_id);
  tabs::TabHandle insert_handle(insert_handle_id);
  auto replace_index =
      tab_strip_model_adapter().GetIndexForHandle(replace_handle);
  auto insert_index =
      tab_strip_model_adapter().GetIndexForHandle(insert_handle);

  if (!replace_index.has_value() || !insert_index.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tabs"));
  }

  tab_strip_model_adapter().ReplaceTabInSplit(replace_handle,
                                              insert_index.value());

  return std::monostate();
}

void TabStripServiceImpl::Accept(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) {
  mojo_clients_.Add(&bridge_, std::move(client));
}

void TabStripServiceImpl::AcceptExperimental(
    mojo::PendingReceiver<tabs_api::mojom::TabStripExperimentService> client) {
  mojo_experiment_clients_.Add(&experimental_bridge_, std::move(client));
}

}  // namespace tabs_api

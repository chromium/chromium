// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api::converters {

mojom::NetworkState ToMojo(TabNetworkState state) {
  switch (state) {
    case TabNetworkState::kNone:
      return mojom::NetworkState::kNone;
    case TabNetworkState::kWaiting:
      return mojom::NetworkState::kWaiting;
    case TabNetworkState::kLoading:
      return mojom::NetworkState::kLoading;
    case TabNetworkState::kError:
      return mojom::NetworkState::kError;
  }
}

mojom::AlertState ToMojo(tabs::TabAlert state) {
  switch (state) {
    case tabs::TabAlert::kMediaRecording:
      return mojom::AlertState::kMediaRecording;
    case tabs::TabAlert::kTabCapturing:
      return mojom::AlertState::kTabCapturing;
    case tabs::TabAlert::kAudioPlaying:
      return mojom::AlertState::kAudioPlaying;
    case tabs::TabAlert::kAudioMuting:
      return mojom::AlertState::kAudioMuting;
    case tabs::TabAlert::kBluetoothConnected:
      return mojom::AlertState::kBluetoothConnected;
    case tabs::TabAlert::kBluetoothScanActive:
      return mojom::AlertState::kBluetoothScanActive;
    case tabs::TabAlert::kUsbConnected:
      return mojom::AlertState::kUsbConnected;
    case tabs::TabAlert::kHidConnected:
      return mojom::AlertState::kHidConnected;
    case tabs::TabAlert::kSerialConnected:
      return mojom::AlertState::kSerialConnected;
    case tabs::TabAlert::kPipPlaying:
      return mojom::AlertState::kPipPlaying;
    case tabs::TabAlert::kDesktopCapturing:
      return mojom::AlertState::kDesktopCapturing;
    case tabs::TabAlert::kVrPresentingInHeadset:
      return mojom::AlertState::kVrPresentingInHeadset;
    case tabs::TabAlert::kAudioRecording:
      return mojom::AlertState::kAudioRecording;
    case tabs::TabAlert::kVideoRecording:
      return mojom::AlertState::kVideoRecording;
    case tabs::TabAlert::kGlicAccessing:
      return mojom::AlertState::kGlicAccessing;
    case tabs::TabAlert::kGlicSharing:
      return mojom::AlertState::kGlicSharing;
    case tabs::TabAlert::kActorAccessing:
      return mojom::AlertState::kActorAccessing;
    case tabs::TabAlert::kActorWaitingOnUser:
      return mojom::AlertState::kActorWaitingOnUser;
  }
}

std::vector<mojom::AlertState> ToMojo(
    const std::vector<tabs::TabAlert>& states) {
  std::vector<mojom::AlertState> result;
  result.reserve(states.size());
  for (auto state : states) {
    result.push_back(ToMojo(state));
  }
  return result;
}

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabHandle handle,
                                     const TabRendererData& data,
                                     const ui::ColorProvider& color_provider,
                                     const TabStates& states) {
  auto result = tabs_api::mojom::Tab::New();

  result->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
  result->title = base::UTF16ToUTF8(data.title);
  result->favicon = data.favicon.Rasterize(&color_provider);
  result->url = data.visible_url;
  result->network_state = ToMojo(data.network_state);
  if (handle.Get() != nullptr) {
    result->alert_states = ToMojo(
        tabs::TabAlertController::From(handle.Get())->GetAllActiveAlerts());
  }

  result->is_active = states.is_active;
  result->is_selected = states.is_selected;
  result->is_blocked = data.blocked;

  return result;
}

tabs_api::mojom::DataPtr BuildMojoTabCollectionData(
    tabs::TabCollectionHandle handle) {
  const tabs::TabCollection* collection = handle.Get();
  CHECK(collection);
  auto node_id = tabs_api::NodeId(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(collection->GetHandle().raw_value()));
  switch (collection->type()) {
    case tabs::TabCollection::Type::TABSTRIP: {
      auto mojo_tab_strip = tabs_api::mojom::TabStrip::New();
      mojo_tab_strip->id = node_id;
      return tabs_api::mojom::Data::NewTabStrip(std::move(mojo_tab_strip));
    }
    case tabs::TabCollection::Type::PINNED: {
      auto mojo_pinned_tabs = tabs_api::mojom::PinnedTabs::New();
      mojo_pinned_tabs->id = node_id;
      return tabs_api::mojom::Data::NewPinnedTabs(std::move(mojo_pinned_tabs));
    }
    case tabs::TabCollection::Type::UNPINNED: {
      auto mojo_unpinned_tabs = tabs_api::mojom::UnpinnedTabs::New();
      mojo_unpinned_tabs->id = node_id;
      return tabs_api::mojom::Data::NewUnpinnedTabs(
          std::move(mojo_unpinned_tabs));
    }
    case tabs::TabCollection::Type::GROUP: {
      auto mojo_tab_group = tabs_api::mojom::TabGroup::New();
      mojo_tab_group->id = node_id;
      const tabs::TabGroupTabCollection* group_collection =
          static_cast<const tabs::TabGroupTabCollection*>(collection);
      const TabGroup* tab_group = group_collection->GetTabGroup();
      CHECK(tab_group);
      mojo_tab_group->data = *tab_group->visual_data();
      return tabs_api::mojom::Data::NewTabGroup(std::move(mojo_tab_group));
    }
    case tabs::TabCollection::Type::SPLIT: {
      auto mojo_split_tab = tabs_api::mojom::SplitTab::New();
      mojo_split_tab->id = node_id;
      const tabs::SplitTabCollection* split_collection =
          static_cast<const tabs::SplitTabCollection*>(collection);
      split_tabs::SplitTabData* split_data = split_collection->data();
      CHECK(split_data);
      split_tabs::SplitTabVisualData* visual_data = split_data->visual_data();
      CHECK(visual_data);
      mojo_split_tab->data = *visual_data;
      return tabs_api::mojom::Data::NewSplitTab(std::move(mojo_split_tab));
    }
  }
  NOTREACHED();
}

TabNetworkState FromMojo(mojom::NetworkState state) {
  switch (state) {
    case mojom::NetworkState::kNone:
      return TabNetworkState::kNone;
    case mojom::NetworkState::kWaiting:
      return TabNetworkState::kWaiting;
    case mojom::NetworkState::kLoading:
      return TabNetworkState::kLoading;
    case mojom::NetworkState::kError:
      return TabNetworkState::kError;
  }
}

tabs::TabAlert FromMojo(mojom::AlertState state) {
  switch (state) {
    case mojom::AlertState::kMediaRecording:
      return tabs::TabAlert::kMediaRecording;
    case mojom::AlertState::kTabCapturing:
      return tabs::TabAlert::kTabCapturing;
    case mojom::AlertState::kAudioPlaying:
      return tabs::TabAlert::kAudioPlaying;
    case mojom::AlertState::kAudioMuting:
      return tabs::TabAlert::kAudioMuting;
    case mojom::AlertState::kBluetoothConnected:
      return tabs::TabAlert::kBluetoothConnected;
    case mojom::AlertState::kBluetoothScanActive:
      return tabs::TabAlert::kBluetoothScanActive;
    case mojom::AlertState::kUsbConnected:
      return tabs::TabAlert::kUsbConnected;
    case mojom::AlertState::kHidConnected:
      return tabs::TabAlert::kHidConnected;
    case mojom::AlertState::kSerialConnected:
      return tabs::TabAlert::kSerialConnected;
    case mojom::AlertState::kPipPlaying:
      return tabs::TabAlert::kPipPlaying;
    case mojom::AlertState::kDesktopCapturing:
      return tabs::TabAlert::kDesktopCapturing;
    case mojom::AlertState::kVrPresentingInHeadset:
      return tabs::TabAlert::kVrPresentingInHeadset;
    case mojom::AlertState::kAudioRecording:
      return tabs::TabAlert::kAudioRecording;
    case mojom::AlertState::kVideoRecording:
      return tabs::TabAlert::kVideoRecording;
    case mojom::AlertState::kGlicAccessing:
      return tabs::TabAlert::kGlicAccessing;
    case mojom::AlertState::kGlicSharing:
      return tabs::TabAlert::kGlicSharing;
    case mojom::AlertState::kActorAccessing:
      return tabs::TabAlert::kActorAccessing;
    case mojom::AlertState::kActorWaitingOnUser:
      return tabs::TabAlert::kActorWaitingOnUser;
  }
}

}  // namespace tabs_api::converters

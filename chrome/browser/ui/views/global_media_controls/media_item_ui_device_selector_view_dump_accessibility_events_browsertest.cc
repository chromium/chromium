// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/media_message_center/notification_theme.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using global_media_controls::test::MockDeviceListHost;

namespace {

constexpr char kItemId[] = "item_id";

// Stub delegate that satisfies the MediaItemUIDeviceSelectorDelegate interface
// without performing any real work.
class StubMediaItemUIDeviceSelectorDelegate
    : public MediaItemUIDeviceSelectorDelegate {
 public:
  StubMediaItemUIDeviceSelectorDelegate() = default;
  ~StubMediaItemUIDeviceSelectorDelegate() override = default;

  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override {}
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override {
    return {};
  }
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback) override {
    return {};
  }
  void OnMediaRemotingRequested(const std::string& item_id) override {}
};

}  // namespace

namespace views {
namespace {

class MediaItemUIDeviceSelectorViewDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters;

#if BUILDFLAG(IS_WIN)
    filters.emplace_back("EVENT_OBJECT_STATECHANGE*",
                         ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("ExpandCollapseExpandCollapseState*",
                         ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("StructureChanged*", ui::AXPropertyFilter::DENY);
    filters.emplace_back("AriaProperties*", ui::AXPropertyFilter::DENY);
#elif BUILDFLAG(IS_MAC)
    filters.emplace_back("AXExpandedChanged*", ui::AXPropertyFilter::ALLOW);
#endif

    return filters;
  }

  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    device_list_host_ = std::make_unique<MockDeviceListHost>();

    auto device_selector_view = std::make_unique<MediaItemUIDeviceSelectorView>(
        kItemId, &delegate_, device_list_host_->PassRemote(),
        client_remote_.BindNewPipeAndPassReceiver(),
        /*has_audio_output=*/false,
        global_media_controls::GlobalMediaControlsEntryPoint::kToolbarIcon,
        media_message_center::MediaColorTheme(),
        /*show_devices=*/false);
    device_selector_view->GetViewAccessibility().SetRole(
        ax::mojom::Role::kGroup);
    device_selector_view->GetViewAccessibility().SetName(u"Device Selector");
    device_selector_view_ =
        container->AddChildView(std::move(device_selector_view));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    device_selector_view_ = nullptr;
    device_list_host_.reset();
    client_remote_.reset();
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<MediaItemUIDeviceSelectorView> device_selector_view_ = nullptr;
  StubMediaItemUIDeviceSelectorDelegate delegate_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_remote_;
  std::unique_ptr<MockDeviceListHost> device_list_host_;
};

IN_PROC_BROWSER_TEST_P(MediaItemUIDeviceSelectorViewDumpAccessibilityEventsTest,
                       ShowDevices) {
  AddAllowFilter("STATE-CHANGE:EXPANDED:TRUE*");
  BEGIN_RECORDING_EVENTS_OR_SKIP("media-device-selector-show-devices");
  device_selector_view_->ShowDevices();
}

IN_PROC_BROWSER_TEST_P(MediaItemUIDeviceSelectorViewDumpAccessibilityEventsTest,
                       HideDevices) {
  device_selector_view_->ShowDevices();
  // Flush the pending serialization so BrowserAccessibilityManager processes
  // the expanded state before recording starts. Without this, the expanded and
  // collapsed state changes batch into one serialization and AXEventGenerator
  // sees no net change.
  WaitForPendingSerialization();

  AddAllowFilter("STATE-CHANGE:EXPANDED:FALSE*");
  BEGIN_RECORDING_EVENTS_OR_SKIP("media-device-selector-hide-devices");
  device_selector_view_->HideDevices();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaItemUIDeviceSelectorViewDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views

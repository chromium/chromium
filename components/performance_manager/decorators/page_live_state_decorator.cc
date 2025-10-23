// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/performance_manager/decorators/decorators_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

namespace {

// Private implementation of the node attached data. This keeps the complexity
// out of the header file.
class PageLiveStateDataImpl
    : public PageLiveStateDecorator::Data,
      public NodeAttachedDataImpl<PageLiveStateDataImpl> {
 public:
  explicit PageLiveStateDataImpl(const PageNodeImpl* page_node)
      : page_node_(page_node) {}

  ~PageLiveStateDataImpl() override = default;
  PageLiveStateDataImpl(const PageLiveStateDataImpl& other) = delete;
  PageLiveStateDataImpl& operator=(const PageLiveStateDataImpl&) = delete;

  // PageLiveStateDecorator::Data:
  bool IsConnectedToUSBDevice() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_usb_device_;
  }
  bool IsConnectedToBluetoothDevice() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_bluetooth_device_;
  }
  bool IsConnectedToHidDevice() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_hid_device_;
  }
  bool IsConnectedToSerialPort() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_connected_to_serial_port_;
  }
  bool IsCapturingVideo() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_video_;
  }
  bool IsCapturingAudio() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_audio_;
  }
  bool IsBeingMirrored() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_being_mirrored_;
  }
  bool IsCapturingWindow() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_window_;
  }
  bool IsCapturingDisplay() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_capturing_display_;
  }
  bool IsDiscarded() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_discarded_;
  }
  bool IsAutoDiscardable() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_auto_discardable_;
  }
  bool IsActiveTab() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_active_tab_;
  }
  bool IsPinnedTab() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_pinned_tab_;
  }
  bool IsDevToolsOpen() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_dev_tools_open_;
  }
  bool UpdatedTitleOrFaviconInBackground() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return updated_title_or_favicon_in_background_;
  }

  void SetIsConnectedToUSBDeviceForTesting(bool value) override {
    set_is_connected_to_usb_device(value);
  }
  void SetIsConnectedToBluetoothDeviceForTesting(bool value) override {
    set_is_connected_to_bluetooth_device(value);
  }
  void SetIsConnectedToHidDeviceForTesting(bool value) override {
    set_is_connected_to_hid_device(value);
  }
  void SetIsConnectedToSerialPortForTesting(bool value) override {
    set_is_connected_to_serial_port(value);
  }
  void SetIsCapturingVideoForTesting(bool value) override {
    set_is_capturing_video(value);
  }
  void SetIsCapturingAudioForTesting(bool value) override {
    set_is_capturing_audio(value);
  }
  void SetIsBeingMirroredForTesting(bool value) override {
    set_is_being_mirrored(value);
  }
  void SetIsCapturingWindowForTesting(bool value) override {
    set_is_capturing_window(value);
  }
  void SetIsCapturingDisplayForTesting(bool value) override {
    set_is_capturing_display(value);
  }
  void SetIsDiscardedForTesting(bool value) override {
    set_is_discarded(value);
  }
  void SetIsAutoDiscardableForTesting(bool value) override {
    set_is_auto_discardable(value);
  }
  void SetIsActiveTabForTesting(bool value) override {
    set_is_active_tab(value);
  }
  void SetIsPinnedTabForTesting(bool value) override {
    set_is_pinned_tab(value);
  }
  void SetIsDevToolsOpenForTesting(bool value) override {
    set_is_dev_tools_open(value);
  }
  void SetUpdatedTitleOrFaviconInBackgroundForTesting(bool value) override {
    set_updated_title_or_favicon_in_background(value);
  }

  void set_is_connected_to_usb_device(bool is_connected_to_usb_device) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_usb_device_ == is_connected_to_usb_device)
      return;
    is_connected_to_usb_device_ = is_connected_to_usb_device;
    for (auto& obs : observers_)
      obs.OnIsConnectedToUSBDeviceChanged(page_node_);
  }
  void set_is_connected_to_bluetooth_device(
      bool is_connected_to_bluetooth_device) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_bluetooth_device_ == is_connected_to_bluetooth_device)
      return;
    is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
    for (auto& obs : observers_)
      obs.OnIsConnectedToBluetoothDeviceChanged(page_node_);
  }
  void set_is_connected_to_hid_device(bool is_connected_to_hid_device) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_hid_device_ == is_connected_to_hid_device) {
      return;
    }
    is_connected_to_hid_device_ = is_connected_to_hid_device;
    for (auto& obs : observers_) {
      obs.OnIsConnectedToHidDeviceChanged(page_node_);
    }
  }
  void set_is_connected_to_serial_port(bool is_connected_to_serial_port) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_connected_to_serial_port_ == is_connected_to_serial_port) {
      return;
    }
    is_connected_to_serial_port_ = is_connected_to_serial_port;
    for (auto& obs : observers_) {
      obs.OnIsConnectedToSerialPortChanged(page_node_);
    }
  }
  void set_is_capturing_video(bool is_capturing_video) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_video_ == is_capturing_video)
      return;
    is_capturing_video_ = is_capturing_video;
    for (auto& obs : observers_)
      obs.OnIsCapturingVideoChanged(page_node_);
  }
  void set_is_capturing_audio(bool is_capturing_audio) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_audio_ == is_capturing_audio)
      return;
    is_capturing_audio_ = is_capturing_audio;
    for (auto& obs : observers_)
      obs.OnIsCapturingAudioChanged(page_node_);
  }
  void set_is_being_mirrored(bool is_being_mirrored) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_being_mirrored_ == is_being_mirrored)
      return;
    is_being_mirrored_ = is_being_mirrored;
    for (auto& obs : observers_)
      obs.OnIsBeingMirroredChanged(page_node_);
  }
  void set_is_capturing_window(bool is_capturing_window) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_window_ == is_capturing_window)
      return;
    is_capturing_window_ = is_capturing_window;
    for (auto& obs : observers_)
      obs.OnIsCapturingWindowChanged(page_node_);
  }
  void set_is_capturing_display(bool is_capturing_display) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_capturing_display_ == is_capturing_display)
      return;
    is_capturing_display_ = is_capturing_display;
    for (auto& obs : observers_)
      obs.OnIsCapturingDisplayChanged(page_node_);
  }
  void set_is_discarded(bool is_discarded) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_discarded_ = is_discarded;
  }
  void set_is_auto_discardable(bool is_auto_discardable) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_auto_discardable_ == is_auto_discardable)
      return;
    is_auto_discardable_ = is_auto_discardable;
    for (auto& obs : observers_)
      obs.OnIsAutoDiscardableChanged(page_node_);
  }
  void set_is_active_tab(bool is_active_tab) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_active_tab_ == is_active_tab)
      return;
    is_active_tab_ = is_active_tab;
    for (auto& obs : observers_)
      obs.OnIsActiveTabChanged(page_node_);
  }
  void set_is_pinned_tab(bool is_pinned_tab) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_pinned_tab_ == is_pinned_tab) {
      return;
    }
    is_pinned_tab_ = is_pinned_tab;
    for (auto& obs : observers_) {
      obs.OnIsPinnedTabChanged(page_node_);
    }
  }
  void set_is_dev_tools_open(bool is_dev_tools_open) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_dev_tools_open_ == is_dev_tools_open) {
      return;
    }
    is_dev_tools_open_ = is_dev_tools_open;
    for (auto& obs : observers_) {
      obs.OnIsDevToolsOpenChanged(page_node_);
    }
  }
  void set_updated_title_or_favicon_in_background(bool updated) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (updated_title_or_favicon_in_background_ == updated) {
      return;
    }
    updated_title_or_favicon_in_background_ = updated;
    for (auto& obs : observers_) {
      obs.OnUpdatedTitleOrFaviconInBackgroundChanged(page_node_);
    }
  }

 private:
  bool is_connected_to_usb_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_connected_to_bluetooth_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_connected_to_hid_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_connected_to_serial_port_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_capturing_video_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_audio_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_being_mirrored_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_window_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_display_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_discarded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_auto_discardable_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
  bool is_active_tab_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_pinned_tab_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_dev_tools_open_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool updated_title_or_favicon_in_background_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;

  const raw_ptr<const PageNode> page_node_;
};

const char kDescriberName[] = "PageLiveStateDecorator";

}  // namespace

PageLiveStateDecorator::PageLiveStateDecorator() = default;
PageLiveStateDecorator::~PageLiveStateDecorator() = default;

// static
void PageLiveStateDecorator::AddAllPageObserver(
    PageLiveStateObserver* observer) {
  // Must not be called before PerformanceManager is available, or observations
  // will be lost.
  CHECK(PerformanceManager::IsAvailable());
  auto* graph = PerformanceManager::GetGraph();
  PageLiveStateDecorator::GetFromGraph(graph)->all_page_observers_.AddObserver(
      observer);
  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node))
        ->AddObserver(observer);
  }
}

// static
void PageLiveStateDecorator::RemoveAllPageObserver(
    PageLiveStateObserver* observer) {
  if (!PerformanceManager::IsAvailable()) {
    // Observer list was already cleared when PageLiveStateDecorator was
    // destroyed.
    return;
  }
  auto* graph = PerformanceManager::GetGraph();
  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    auto* data = PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node));
    // `data` might be null if AddAllPageObserver was never called (which is
    // possible because, by the semantics of ObserverList, it's legal to remove
    // an observer that was never added), or if RemoveAllPageObserver is being
    // called from an OnPageNodeAdded implementation that happens to be called
    // before PageLiveStateDecorator::OnPageNodeAdded.
    if (data) {
      data->RemoveObserver(observer);
    }
  }
  PageLiveStateDecorator::GetFromGraph(graph)
      ->all_page_observers_.RemoveObserver(observer);
}

// static
bool PageLiveStateDecorator::HasAllPageObserver(
    PageLiveStateObserver* observer) {
  if (!PerformanceManager::IsAvailable()) {
    // Observer list was cleared when PageLiveStateDecorator was destroyed.
    return false;
  }
  return PageLiveStateDecorator::GetFromGraph(PerformanceManager::GetGraph())
      ->all_page_observers_.HasObserver(observer);
}

// static
void PageLiveStateDecorator::OnCapabilityTypesChanged(
    content::WebContents* contents,
    content::WebContentsCapabilityType capability_type,
    bool used) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (capability_type) {
    case content::WebContentsCapabilityType::kUSB:
      SetPropertyForWebContentsPageNode(
          contents, &PageLiveStateDataImpl::set_is_connected_to_usb_device,
          used);
      break;
    case content::WebContentsCapabilityType::kBluetoothConnected:
      SetPropertyForWebContentsPageNode(
          contents,
          &PageLiveStateDataImpl::set_is_connected_to_bluetooth_device, used);
      break;
    case content::WebContentsCapabilityType::kHID:
      SetPropertyForWebContentsPageNode(
          contents, &PageLiveStateDataImpl::set_is_connected_to_hid_device,
          used);
      break;
    case content::WebContentsCapabilityType::kSerial:
      SetPropertyForWebContentsPageNode(
          contents, &PageLiveStateDataImpl::set_is_connected_to_serial_port,
          used);
      break;
    default:
      break;
  }
}

// static
void PageLiveStateDecorator::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_video,
      is_capturing_video);
}

// static
void PageLiveStateDecorator::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_audio,
      is_capturing_audio);
}

// static
void PageLiveStateDecorator::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_being_mirrored,
      is_being_mirrored);
}

// static
void PageLiveStateDecorator::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_window,
      is_capturing_window);
}

// static
void PageLiveStateDecorator::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_display,
      is_capturing_display);
}

// static
void PageLiveStateDecorator::SetIsDiscarded(content::WebContents* contents,
                                            bool is_discarded) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_discarded, is_discarded);
}

// static
void PageLiveStateDecorator::SetIsAutoDiscardable(
    content::WebContents* contents,
    bool is_auto_discardable) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_auto_discardable,
      is_auto_discardable);
}

// static
void PageLiveStateDecorator::SetIsActiveTab(content::WebContents* contents,
                                            bool is_active_tab) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_active_tab, is_active_tab);
}

// static
void PageLiveStateDecorator::SetIsPinnedTab(content::WebContents* contents,
                                            bool is_pinned_tab) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_pinned_tab, is_pinned_tab);
}

// static
void PageLiveStateDecorator::SetIsDevToolsOpen(content::WebContents* contents,
                                               bool is_dev_tools_open) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_dev_tools_open,
      is_dev_tools_open);
}

// static
bool PageLiveStateDecorator::IsConnectedToUSBDevice(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsConnectedToUSBDevice);
}

// static
bool PageLiveStateDecorator::IsConnectedToBluetoothDevice(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsConnectedToBluetoothDevice);
}

// static
bool PageLiveStateDecorator::IsConnectedToHidDevice(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsConnectedToHidDevice);
}

// static
bool PageLiveStateDecorator::IsConnectedToSerialPort(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsConnectedToSerialPort);
}

// static
bool PageLiveStateDecorator::IsCapturingVideo(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsCapturingVideo);
}

// static
bool PageLiveStateDecorator::IsCapturingAudio(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsCapturingAudio);
}

// static
bool PageLiveStateDecorator::IsBeingMirrored(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsBeingMirrored);
}

// static
bool PageLiveStateDecorator::IsCapturingWindow(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsCapturingWindow);
}

// static
bool PageLiveStateDecorator::IsCapturingDisplay(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsCapturingDisplay);
}

// static
bool PageLiveStateDecorator::IsDiscarded(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsDiscarded);
}

// static
bool PageLiveStateDecorator::IsAutoDiscardable(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsAutoDiscardable);
}

// static
bool PageLiveStateDecorator::IsActiveTab(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsActiveTab);
}

// static
bool PageLiveStateDecorator::IsPinnedTab(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsPinnedTab);
}

// static
bool PageLiveStateDecorator::IsDevToolsOpen(content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::IsDevToolsOpen);
}

// static
bool PageLiveStateDecorator::UpdatedTitleOrFaviconInBackground(
    content::WebContents* contents) {
  return GetPropertyForWebContentsPageNode<bool>(
      contents, &PageLiveStateDataImpl::UpdatedTitleOrFaviconInBackground);
}

void PageLiveStateDecorator::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
  graph->AddPageNodeObserver(this);
}

void PageLiveStateDecorator::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value::Dict PageLiveStateDecorator::DescribePageNodeData(
    const PageNode* node) const {
  auto* data = Data::FromPageNode(node);
  if (!data)
    return base::Value::Dict();

  base::Value::Dict ret;
  ret.Set("IsConnectedToUSBDevice", data->IsConnectedToUSBDevice());
  ret.Set("IsConnectedToBluetoothDevice", data->IsConnectedToBluetoothDevice());
  ret.Set("IsConnectedToHidDevice", data->IsConnectedToHidDevice());
  ret.Set("IsConnectedToSerialPort", data->IsConnectedToSerialPort());
  ret.Set("IsCapturingVideo", data->IsCapturingVideo());
  ret.Set("IsCapturingAudio", data->IsCapturingAudio());
  ret.Set("IsBeingMirrored", data->IsBeingMirrored());
  ret.Set("IsCapturingWindow", data->IsCapturingWindow());
  ret.Set("IsCapturingDisplay", data->IsCapturingDisplay());
  ret.Set("IsDiscarded", data->IsDiscarded());
  ret.Set("IsAutoDiscardable", data->IsAutoDiscardable());
  ret.Set("IsActiveTab", data->IsActiveTab());
  ret.Set("IsPinnedTab", data->IsPinnedTab());
  ret.Set("IsDevToolsOpen", data->IsDevToolsOpen());
  ret.Set("UpdatedTitleOrFaviconInBackground",
          data->UpdatedTitleOrFaviconInBackground());

  return ret;
}

void PageLiveStateDecorator::OnPageNodeAdded(const PageNode* page_node) {
  if (all_page_observers_.empty()) {
    return;
  }
  auto* data =
      PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
  for (PageLiveStateObserver& observer : all_page_observers_) {
    data->AddObserver(&observer);
  }
}

void PageLiveStateDecorator::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  if (all_page_observers_.empty()) {
    return;
  }
  auto* data = PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node));
  // Since an observer exists, AddAllPageObservers was called. So `data` was
  // created either there or in OnPageNodeAdded.
  CHECK(data);
  for (PageLiveStateObserver& observer : all_page_observers_) {
    data->RemoveObserver(&observer);
  }
}

void PageLiveStateDecorator::OnTitleUpdated(const PageNode* page_node) {
  if (!page_node->IsVisible()) {
    PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node))
        ->set_updated_title_or_favicon_in_background(true);
  }
}

void PageLiveStateDecorator::OnFaviconUpdated(const PageNode* page_node) {
  if (!page_node->IsVisible()) {
    PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node))
        ->set_updated_title_or_favicon_in_background(true);
  }
}

void PageLiveStateDecorator::OnAboutToBeDiscarded(
    const PageNode* page_node,
    const PageNode* new_page_node) {
  if (const auto* data =
          PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node))) {
    // IsAutoDiscardable is a tab property so applies to the new PageNode too.
    PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(new_page_node))
        ->set_is_auto_discardable(data->IsAutoDiscardable());
  }
}

PageLiveStateDecorator::Data::Data() = default;
PageLiveStateDecorator::Data::~Data() = default;

void PageLiveStateDecorator::Data::AddObserver(
    PageLiveStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void PageLiveStateDecorator::Data::RemoveObserver(
    PageLiveStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

const PageLiveStateDecorator::Data* PageLiveStateDecorator::Data::FromPageNode(
    const PageNode* page_node) {
  return PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node));
}

PageLiveStateDecorator::Data*
PageLiveStateDecorator::Data::GetOrCreateForPageNode(
    const PageNode* page_node) {
  return PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

PageLiveStateObserver::PageLiveStateObserver() = default;
PageLiveStateObserver::~PageLiveStateObserver() = default;

}  // namespace performance_manager

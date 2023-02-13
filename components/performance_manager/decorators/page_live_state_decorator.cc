// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/decorators/decorators_utils.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
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
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
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
  bool IsAutoDiscardable() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_auto_discardable_;
  }
  bool WasDiscarded() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return was_discarded_;
  }
  bool IsActiveTab() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_active_tab_;
  }
  bool IsPinnedTab() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_pinned_tab_;
  }
  bool IsContentSettingTypeAllowed(ContentSettingsType type) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = content_settings_.find(type);
    return it != content_settings_.end() && it->second == CONTENT_SETTING_ALLOW;
  }
  bool IsDevToolsOpen() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_dev_tools_open_;
  }

  void SetIsConnectedToUSBDeviceForTesting(bool value) override {
    set_is_connected_to_usb_device(value);
  }
  void SetIsConnectedToBluetoothDeviceForTesting(bool value) override {
    set_is_connected_to_bluetooth_device(value);
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
  void SetIsAutoDiscardableForTesting(bool value) override {
    set_is_auto_discardable(value);
  }
  void SetWasDiscardedForTesting(bool value) override {
    set_was_discarded(value);
  }
  void SetIsActiveTabForTesting(bool value) override {
    set_is_active_tab(value);
  }
  void SetIsPinnedTabForTesting(bool value) override {
    set_is_pinned_tab(value);
  }
  void SetContentSettingsForTesting(
      const std::map<ContentSettingsType, ContentSetting>& settings) override {
    set_content_settings(settings);
  }
  void SetIsDevToolsOpenForTesting(bool value) override {
    set_is_dev_tools_open(value);
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
  void set_is_auto_discardable(bool is_auto_discardable) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (is_auto_discardable_ == is_auto_discardable)
      return;
    is_auto_discardable_ = is_auto_discardable;
    for (auto& obs : observers_)
      obs.OnIsAutoDiscardableChanged(page_node_);
  }
  void set_was_discarded(bool was_discarded) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (was_discarded_ == was_discarded)
      return;
    was_discarded_ = was_discarded;
    for (auto& obs : observers_)
      obs.OnWasDiscardedChanged(page_node_);
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
  void set_content_settings(
      std::map<ContentSettingsType, ContentSetting> settings) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Content settings are set for the first time when the page navigates, and
    // subsequently when a notification that they have changed is received.
    // Therefore, no need to check to see if they're equal to the previous
    // value.
    content_settings_ = std::move(settings);
    for (auto& obs : observers_)
      obs.OnContentSettingsChanged(page_node_);
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

 private:
  // Make the impl our friend so it can access the constructor and any
  // storage providers.
  friend class ::performance_manager::NodeAttachedDataImpl<
      PageLiveStateDataImpl>;

  explicit PageLiveStateDataImpl(const PageNodeImpl* page_node)
      : page_node_(page_node) {}

  bool is_connected_to_usb_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_connected_to_bluetooth_device_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool is_capturing_video_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_audio_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_being_mirrored_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_window_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_capturing_display_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_auto_discardable_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
  bool was_discarded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_active_tab_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_pinned_tab_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::map<ContentSettingsType, ContentSetting> content_settings_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_dev_tools_open_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  const raw_ptr<const PageNode> page_node_;
};

const char kDescriberName[] = "PageLiveStateDecorator";

}  // namespace

void PageLiveStateDecorator::Delegate::GetContentSettingsAndReply(
    WebContentsProxy web_contents_proxy,
    const GURL& url,
    GetContentSettingsForUrlCallback callback) {
  content::WebContents* web_contents = web_contents_proxy.Get();
  if (web_contents) {
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents),
            GetContentSettingsForUrl(web_contents, url)));
  }
}

PageLiveStateDecorator::PageLiveStateDecorator(
    base::SequenceBound<Delegate> delegate)
    : delegate_(std::move(delegate)) {}
PageLiveStateDecorator::~PageLiveStateDecorator() = default;

// static
void PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_usb_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_usb_device,
      is_connected_to_usb_device);
}

// static
void PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_bluetooth_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_bluetooth_device,
      is_connected_to_bluetooth_device);
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
void PageLiveStateDecorator::SetIsAutoDiscardable(
    content::WebContents* contents,
    bool is_auto_discardable) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_auto_discardable,
      is_auto_discardable);
}

// static
void PageLiveStateDecorator::SetWasDiscarded(content::WebContents* contents,
                                             bool was_discarded) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_was_discarded, was_discarded);
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
void PageLiveStateDecorator::SetContentSettings(
    content::WebContents* contents,
    std::map<ContentSettingsType, ContentSetting> settings) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_content_settings, settings);
}

// static
void PageLiveStateDecorator::SetIsDevToolsOpen(content::WebContents* contents,
                                               bool is_dev_tools_open) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_dev_tools_open,
      is_dev_tools_open);
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
  ret.Set("IsCapturingVideo", data->IsCapturingVideo());
  ret.Set("IsCapturingAudio", data->IsCapturingAudio());
  ret.Set("IsBeingMirrored", data->IsBeingMirrored());
  ret.Set("IsCapturingWindow", data->IsCapturingWindow());
  ret.Set("IsCapturingDisplay", data->IsCapturingDisplay());
  ret.Set("IsAutoDiscardable", data->IsAutoDiscardable());
  ret.Set("WasDiscarded", data->WasDiscarded());
  ret.Set("IsActiveTab", data->IsActiveTab());

  return ret;
}

void PageLiveStateDecorator::OnMainFrameUrlChanged(const PageNode* page_node) {
  // Don't get the content settings on android on each navigation because it may
  // induce scroll jank. There are many same-document navigations while
  // scrolling and getting the settings can invoke expensive platform APIs on
  // Android. Moreover, this information is only used to decide if a tab should
  // be discarded, which doesn't happen through Chrome code on that platform.
#if !BUILDFLAG(IS_ANDROID)
  // Get the content settings from the main thread.
  // This call is not using `Then` and is instead passing a callback for the
  // delegate to invoke with `CallOnGraph` on purpose. This is because it's
  // possible for the first async call to be placed in the UI thread's task
  // queue, and skipped as part of browser shutdown before being run. When that
  // happens, the post task's reply has to be destroyed on its owner sequence,
  // so a task is posted back to the Performance Manager sequence. At that point
  // shutdown is complete, and the PM sequence is `BLOCK_SHUTDOWN` so a DCHECK
  // is triggered. See crbug.com/1375270.
  delegate_.AsyncCall(&Delegate::GetContentSettingsAndReply)
      .WithArgs(page_node->GetContentsProxy(), page_node->GetMainFrameUrl(),
                base::BindOnce(
                    &PageLiveStateDecorator::OnContentSettingsReceived,
                    weak_factory_.GetWeakPtr(), page_node->GetMainFrameUrl()));
#endif
}

void PageLiveStateDecorator::OnContentSettingsReceived(
    const GURL& url,
    base::WeakPtr<const PageNode> page_node,
    const std::map<ContentSettingsType, ContentSetting>& settings) {
  // If the page node doesn't exist anymore, or it has navigated to a different
  // URL, there's nothing to do.
  if (!page_node || page_node->GetMainFrameUrl() != url) {
    return;
  }

  PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node.get()))
      ->set_content_settings(settings);
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

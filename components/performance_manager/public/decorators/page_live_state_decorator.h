// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PageNode;
class PageLiveStateObserver;

// Used to record some live state information about the PageNode.
// All the functions that take a WebContents* as a parameter should only be
// called from the UI thread, the event will be forwarded to the corresponding
// PageNode on the Performance Manager's sequence.
class PageLiveStateDecorator : public GraphOwnedDefaultImpl,
                               public NodeDataDescriberDefaultImpl,
                               public PageNode::ObserverDefaultImpl {
 public:
  class Data;
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked on the main thread. Returns the relevant content settings for
    // `url` in the web contents' profile.
    virtual std::map<ContentSettingsType, ContentSetting>
    GetContentSettingsForUrl(content::WebContents* web_contents,
                             const GURL& url) = 0;

    using GetContentSettingsForUrlCallback = base::OnceCallback<void(
        base::WeakPtr<const PageNode>,
        const std::map<ContentSettingsType, ContentSetting>&)>;

    void GetContentSettingsAndReply(WebContentsProxy web_contents_proxy,
                                    const GURL& url,
                                    GetContentSettingsForUrlCallback callback);
  };

  // This object should only be used via its static methods.
  explicit PageLiveStateDecorator(base::SequenceBound<Delegate> delegate);
  ~PageLiveStateDecorator() override;
  PageLiveStateDecorator(const PageLiveStateDecorator& other) = delete;
  PageLiveStateDecorator& operator=(const PageLiveStateDecorator&) = delete;

  // Must be called when the connected to USB device state changes.
  static void OnIsConnectedToUSBDeviceChanged(content::WebContents* contents,
                                              bool is_connected_to_usb_device);

  // Must be called when the connected to Bluetooth device state changes.
  static void OnIsConnectedToBluetoothDeviceChanged(
      content::WebContents* contents,
      bool is_connected_to_bluetooth_device);

  // Functions that should be called by a MediaStreamCaptureIndicator::Observer.
  static void OnIsCapturingVideoChanged(content::WebContents* contents,
                                        bool is_capturing_video);
  static void OnIsCapturingAudioChanged(content::WebContents* contents,
                                        bool is_capturing_audio);
  static void OnIsBeingMirroredChanged(content::WebContents* contents,
                                       bool is_being_mirrored);
  static void OnIsCapturingWindowChanged(content::WebContents* contents,
                                         bool is_capturing_window);
  static void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                          bool is_capturing_display);

  // Set the auto discardable property. This indicates whether or not the page
  // can be discarded during an intervention.
  static void SetIsAutoDiscardable(content::WebContents* contents,
                                   bool is_auto_discardable);

  static void SetWasDiscarded(content::WebContents* contents,
                              bool was_discarded);

  static void SetIsActiveTab(content::WebContents* contents,
                             bool is_active_tab);

  static void SetIsPinnedTab(content::WebContents* contents,
                             bool is_pinned_tab);

  static void SetContentSettings(
      content::WebContents* contents,
      std::map<ContentSettingsType, ContentSetting> settings);

  static void SetIsDevToolsOpen(content::WebContents* contents,
                                bool is_dev_tools_open);

 private:
  friend class PageLiveStateDecoratorTest;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // PageNode::ObserverDefaultImpl implementation:
  void OnMainFrameUrlChanged(const PageNode* page_node) override;
  void OnTitleUpdated(const PageNode* page_node) override;
  void OnFaviconUpdated(const PageNode* page_node) override;

  void OnContentSettingsReceived(
      const GURL& url,
      base::WeakPtr<const PageNode> page_node,
      const std::map<ContentSettingsType, ContentSetting>& settings);

  base::SequenceBound<Delegate> delegate_;

  base::WeakPtrFactory<PageLiveStateDecorator> weak_factory_{this};
};

class PageLiveStateDecorator::Data {
 public:
  Data();
  virtual ~Data();
  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  void AddObserver(PageLiveStateObserver* observer);
  void RemoveObserver(PageLiveStateObserver* observer);

  virtual bool IsConnectedToUSBDevice() const = 0;
  virtual bool IsConnectedToBluetoothDevice() const = 0;
  virtual bool IsCapturingVideo() const = 0;
  virtual bool IsCapturingAudio() const = 0;
  virtual bool IsBeingMirrored() const = 0;
  virtual bool IsCapturingWindow() const = 0;
  virtual bool IsCapturingDisplay() const = 0;
  virtual bool IsAutoDiscardable() const = 0;
  virtual bool WasDiscarded() const = 0;
  virtual bool IsActiveTab() const = 0;
  virtual bool IsPinnedTab() const = 0;
  virtual bool IsContentSettingTypeAllowed(ContentSettingsType type) const = 0;
  virtual bool IsDevToolsOpen() const = 0;

  // TODO(https://crbug.com/1418410): Add a notifier for this to
  // PageLiveStateObserver.
  virtual bool UpdatedTitleOrFaviconInBackground() const = 0;

  static const Data* FromPageNode(const PageNode* page_node);
  static Data* GetOrCreateForPageNode(const PageNode* page_node);

  virtual void SetIsConnectedToUSBDeviceForTesting(bool value) = 0;
  virtual void SetIsConnectedToBluetoothDeviceForTesting(bool value) = 0;
  virtual void SetIsCapturingVideoForTesting(bool value) = 0;
  virtual void SetIsCapturingAudioForTesting(bool value) = 0;
  virtual void SetIsBeingMirroredForTesting(bool value) = 0;
  virtual void SetIsCapturingWindowForTesting(bool value) = 0;
  virtual void SetIsCapturingDisplayForTesting(bool value) = 0;
  virtual void SetIsAutoDiscardableForTesting(bool value) = 0;
  virtual void SetWasDiscardedForTesting(bool value) = 0;
  virtual void SetIsActiveTabForTesting(bool value) = 0;
  virtual void SetIsPinnedTabForTesting(bool value) = 0;
  virtual void SetContentSettingsForTesting(
      const std::map<ContentSettingsType, ContentSetting>& settings) = 0;
  virtual void SetIsDevToolsOpenForTesting(bool value) = 0;
  virtual void SetUpdatedTitleOrFaviconInBackgroundForTesting(bool value) = 0;

 protected:
  base::ObserverList<PageLiveStateObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

class PageLiveStateObserver : public base::CheckedObserver {
 public:
  PageLiveStateObserver();
  ~PageLiveStateObserver() override;
  PageLiveStateObserver(const PageLiveStateObserver& other) = delete;
  PageLiveStateObserver& operator=(const PageLiveStateObserver&) = delete;

  virtual void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) = 0;
  virtual void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) = 0;
  virtual void OnIsCapturingVideoChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingAudioChanged(const PageNode* page_node) = 0;
  virtual void OnIsBeingMirroredChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingWindowChanged(const PageNode* page_node) = 0;
  virtual void OnIsCapturingDisplayChanged(const PageNode* page_node) = 0;
  virtual void OnIsAutoDiscardableChanged(const PageNode* page_node) = 0;
  virtual void OnWasDiscardedChanged(const PageNode* page_node) = 0;
  virtual void OnIsActiveTabChanged(const PageNode* page_node) = 0;
  virtual void OnIsPinnedTabChanged(const PageNode* page_node) = 0;
  virtual void OnContentSettingsChanged(const PageNode* page_node) = 0;
  virtual void OnIsDevToolsOpenChanged(const PageNode* page_node) = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_

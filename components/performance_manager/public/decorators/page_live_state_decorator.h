// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PageNode;

// Used to record some live state information about the PageNode.
// All the functions that take a WebContents* as a parameter should only be
// called from the UI thread, the event will be forwarded to the corresponding
// PageNode on the Performance Manager's sequence.
class PageLiveStateDecorator {
 public:
  class Data;

  // This object should only be used via its static methods.
  PageLiveStateDecorator() = delete;
  ~PageLiveStateDecorator() = delete;
  PageLiveStateDecorator(const PageLiveStateDecorator& other) = delete;
  PageLiveStateDecorator& operator=(const PageLiveStateDecorator&) = delete;

  // The following functions should only be called from the UI thread:

  // Should be called whenever a WebContents gets connected or disconnected to
  // a USB device.
  // TODO(sebmarchand|olivierli): Call this from USBTabHelper.
  static void OnWebContentsAttachedToUSBChange(content::WebContents* contents,
                                               bool is_attached_to_usb);
};

class PageLiveStateDecorator::Data {
 public:
  Data();
  virtual ~Data();
  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  virtual bool IsAttachedToUSB() const = 0;

  static Data* GetOrCreateForTesting(PageNode* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LIVE_STATE_DECORATOR_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

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
  bool IsAttachedToUSB() const override { return is_attached_to_usb_; }

  void set_is_attached_to_usb(bool is_attached_to_usb) {
    is_attached_to_usb_ = is_attached_to_usb;
  }

 private:
  // Make the impl our friend so it can access the constructor and any
  // storage providers.
  friend class ::performance_manager::NodeAttachedDataImpl<
      PageLiveStateDataImpl>;

  explicit PageLiveStateDataImpl(const PageNodeImpl* page_node) {}

  bool is_attached_to_usb_ = false;
};

// Helper function to set a property in PageLiveStateDataImpl. This does the
// WebContents -> PageNode translation.
// This can only be called from the UI thread.
template <typename T>
void SetPropertyForWebContents(
    content::WebContents* contents,
    void (PageLiveStateDataImpl::*setter_function)(T),
    T value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<PageNode> node,
                        void (PageLiveStateDataImpl::*setter_function)(T),
                        T value, Graph* graph) {
                       if (node) {
                         auto* data = PageLiveStateDataImpl::GetOrCreate(
                             PageNodeImpl::FromNode(node.get()));
                         DCHECK(data);
                         (data->*setter_function)(value);
                       }
                     },
                     PerformanceManager::GetPageNodeForWebContents(contents),
                     setter_function, value));
}

}  // namespace

// static
void PageLiveStateDecorator::OnWebContentsAttachedToUSBChange(
    content::WebContents* contents,
    bool is_attached_to_usb) {
  SetPropertyForWebContents(contents,
                            &PageLiveStateDataImpl::set_is_attached_to_usb,
                            is_attached_to_usb);
}

PageLiveStateDecorator::Data::Data() = default;
PageLiveStateDecorator::Data::~Data() = default;

PageLiveStateDecorator::Data*
PageLiveStateDecorator::Data::GetOrCreateForTesting(PageNode* page_node) {
  return PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

}  // namespace performance_manager

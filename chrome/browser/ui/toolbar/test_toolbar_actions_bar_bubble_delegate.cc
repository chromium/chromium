// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "ui/base/mojom/dialog_button.mojom.h"

class TestToolbarActionsBarBubbleDelegate::DelegateImpl
    : public ToolbarActionsBarBubbleDelegate {
 public:
  explicit DelegateImpl(TestToolbarActionsBarBubbleDelegate* parent)
      : parent_(parent) {}

  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;

  ~DelegateImpl() override {}

 private:
  bool ShouldShow() override { return !parent_->shown_; }
  std::u16string GetHeadingText() override { return parent_->heading_; }
  std::u16string GetBodyText(bool anchored_to_action) override {
    return parent_->body_;
  }
  std::u16string GetActionButtonText() override { return parent_->action_; }
  std::u16string GetDismissButtonText() override { return parent_->dismiss_; }
  ui::mojom::DialogButton GetDefaultDialogButton() override {
    return parent_->default_button_;
  }
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
  GetExtraViewInfo() override {
    if (parent_->info_)
      return std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>(
          *parent_->info_);
    return nullptr;
  }
  std::string GetAnchorActionId() override { return parent_->action_id_; }
  void OnBubbleShown(base::OnceClosure close_bubble_callback) override {
    CHECK(!parent_->shown_);
    parent_->shown_ = true;
  }
  void OnBubbleClosed(CloseAction action) override {
    CHECK(!parent_->close_action_);
    parent_->close_action_ = std::make_unique<CloseAction>(action);
  }

  raw_ptr<TestToolbarActionsBarBubbleDelegate> parent_;
};

TestToolbarActionsBarBubbleDelegate::TestToolbarActionsBarBubbleDelegate(
    const std::u16string& heading,
    const std::u16string& body,
    const std::u16string& action,
    const std::u16string& dismiss)
    : shown_(false),
      heading_(heading),
      body_(body),
      action_(action),
      dismiss_(dismiss),
      default_button_(ui::mojom::DialogButton::kNone) {}

TestToolbarActionsBarBubbleDelegate::~TestToolbarActionsBarBubbleDelegate() {
  // If the bubble didn't close, it means that it still owns the DelegateImpl,
  // which has a weak ptr to this object. Make sure that this class always
  // outlives the bubble.
  CHECK(close_action_);
}

std::unique_ptr<ToolbarActionsBarBubbleDelegate>
TestToolbarActionsBarBubbleDelegate::GetDelegate() {
  return std::unique_ptr<ToolbarActionsBarBubbleDelegate>(
      new DelegateImpl(this));
}

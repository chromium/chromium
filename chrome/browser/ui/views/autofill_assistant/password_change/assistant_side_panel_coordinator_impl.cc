// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_side_panel_coordinator_impl.h"

#include "base/bind.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "ui/views/layout/box_layout.h"

namespace {
constexpr int kAssistantIconSize = 16;
}  // namespace

std::unique_ptr<AssistantSidePanelCoordinator>
AssistantSidePanelCoordinator::Create(content::WebContents* web_contents) {
  if (SidePanelRegistry::Get(web_contents)
          ->GetEntryForId(SidePanelEntry::Id::kAssistant)) {
    return nullptr;
  }
  return std::make_unique<AssistantSidePanelCoordinatorImpl>(web_contents);
}

AssistantSidePanelCoordinatorImpl::AssistantSidePanelCoordinatorImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // TODO(cr/1322419): Check with designers if this string should be "Google
  // Assistant" as it is in other places. Also make make sure whether it should
  // be translated.
  SidePanelRegistry::Get(web_contents)
      ->Register(std::make_unique<SidePanelEntry>(
          SidePanelEntry::Id::kAssistant, u"Assistant",
          ui::ImageModel::FromVectorIcon(GetAssistantIconOrFallback(),
                                         ui::kColorIcon, kAssistantIconSize),
          base::BindRepeating(
              &AssistantSidePanelCoordinatorImpl::CreateSidePanelView,
              base::Unretained(this))));

  // Listen to `OnEntryHidden` events to be able to propagate them outside.
  GetSidePanelRegistry()
      ->GetEntryForId(SidePanelEntry::Id::kAssistant)
      ->AddObserver(this);

  GetSidePanelCoordinator()->Show(SidePanelEntry::Id::kAssistant);
}

AssistantSidePanelCoordinatorImpl::~AssistantSidePanelCoordinatorImpl() {
  GetSidePanelRegistry()->Deregister(SidePanelEntry::Id::kAssistant);
}

bool AssistantSidePanelCoordinatorImpl::Shown() {
  return side_panel_view_host_;
}

SidePanelCoordinator*
AssistantSidePanelCoordinatorImpl::GetSidePanelCoordinator() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view->side_panel_coordinator();
}

SidePanelRegistry* AssistantSidePanelCoordinatorImpl::GetSidePanelRegistry() {
  return SidePanelRegistry::Get(web_contents_);
}

views::View* AssistantSidePanelCoordinatorImpl::SetView(
    std::unique_ptr<views::View> view) {
  if (!Shown()) {
    // If the side panel view has not been created yet, save this for when
    // creation happens.
    side_panel_view_child_ = std::move(view);
    return side_panel_view_child_.get();
  }
  RemoveView();
  return side_panel_view_host_->AddChildView(std::move(view));
}

views::View* AssistantSidePanelCoordinatorImpl::GetView() {
  if (!Shown())
    return nullptr;
  return !side_panel_view_host_->children().empty()
             ? side_panel_view_host_->children()[0]
             : nullptr;
}

void AssistantSidePanelCoordinatorImpl::RemoveView() {
  side_panel_view_host_->RemoveAllChildViews();
}

void AssistantSidePanelCoordinatorImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AssistantSidePanelCoordinatorImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantSidePanelCoordinatorImpl::OnEntryHidden(SidePanelEntry* entry) {
  side_panel_view_host_ = nullptr;

  for (Observer& observer : observers_)
    observer.OnHidden();
}

std::unique_ptr<views::View>
AssistantSidePanelCoordinatorImpl::CreateSidePanelView() {
  std::unique_ptr<views::View> side_panel_view;
  side_panel_view = std::make_unique<views::View>();
  side_panel_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  if (side_panel_view_child_) {
    side_panel_view->AddChildView(std::move(side_panel_view_child_));
  }
  side_panel_view_host_ = side_panel_view.get();
  return side_panel_view;
}

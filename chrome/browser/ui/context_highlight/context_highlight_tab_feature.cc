// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_tab_feature.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/context_highlight/context_highlight_window_feature.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"

namespace tabs {

ContextHighlightTabFeature::ContextHighlightTabFeature(TabInterface& tab)
    : tab_(&tab), scoped_data_(tab.GetUnownedUserDataHost(), *this) {
  Observe(tab_->GetContents());
  RegisterObserverWithHost(GetRenderWidgetHost());
  discard_subscription_ = tab_->RegisterWillDiscardContents(
      base::BindRepeating(&ContextHighlightTabFeature::OnWillDiscardContents,
                          base::Unretained(this)));
}

ContextHighlightTabFeature* ContextHighlightTabFeature::From(
    TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

ContextHighlightTabFeature::~ContextHighlightTabFeature() {
  UnregisterObserverFromHost(current_host_);
}

content::RenderWidgetHost* ContextHighlightTabFeature::GetRenderWidgetHost()
    const {
  auto* contents = tab_->GetContents();
  auto* rvh = contents->GetRenderViewHost();
  if (!rvh) {
    return nullptr;
  }

  return rvh->GetWidget();
}

void ContextHighlightTabFeature::OnTrackedElementBoundsChanged(
    const cc::TrackedElementBounds& bounds,
    float device_scale_factor) {
  latest_bounds_ = bounds;
  latest_scale_factor_ = device_scale_factor;
  if (tab_->IsActivated()) {
    ContextHighlightWindowFeature* window_feature = GetWindowFeature();
    window_feature->CheckAndUpdateTrackedElementBounds();
  }
}

void ContextHighlightTabFeature::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  content::RenderWidgetHost* old_widget =
      old_host ? old_host->GetWidget() : nullptr;
  content::RenderWidgetHost* new_widget =
      new_host ? new_host->GetWidget() : nullptr;
  RenderWidgetHostChanged(old_widget, new_widget);
}

void ContextHighlightTabFeature::RenderWidgetHostChanged(
    content::RenderWidgetHost* old_host,
    content::RenderWidgetHost* new_host) {
  RegisterObserverWithHost(new_host);
}

void ContextHighlightTabFeature::OnWillDiscardContents(
    TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  latest_bounds_ = cc::TrackedElementBounds();
  UnregisterObserverFromHost(current_host_);
  Observe(new_contents);

  content::RenderWidgetHost* new_host = nullptr;
  if (new_contents && new_contents->GetRenderViewHost()) {
    new_host = new_contents->GetRenderViewHost()->GetWidget();
  }
  RegisterObserverWithHost(new_host);
}

void ContextHighlightTabFeature::RegisterObserverWithHost(
    content::RenderWidgetHost* host) {
  if (current_host_ == host) {
    return;
  }

  UnregisterObserverFromHost(current_host_);
  current_host_ = host;

  if (current_host_) {
    current_host_->AddTrackedElementObserver(this);
  }
}

void ContextHighlightTabFeature::UnregisterObserverFromHost(
    content::RenderWidgetHost* host) {
  if (host) {
    host->RemoveTrackedElementObserver(this);
  }
}

ContextHighlightWindowFeature* ContextHighlightTabFeature::GetWindowFeature() {
  BrowserWindowInterface* browser_window = tab_->GetBrowserWindowInterface();
  return ContextHighlightWindowFeature::From(browser_window);
}

DEFINE_USER_DATA(ContextHighlightTabFeature);

}  // namespace tabs

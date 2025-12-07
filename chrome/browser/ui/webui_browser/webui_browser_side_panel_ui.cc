// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"

#include <optional>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

content::WebContents* GetContainingWebContents(views::View* view) {
  auto* web_view = view->GetViewByID(SidePanelWebUIView::kSidePanelWebViewId);
  CHECK(web_view);
  return views::AsViewClass<views::WebView>(web_view)->web_contents();
}

}  // namespace

WebUIBrowserSidePanelUI::WebUIBrowserSidePanelUI(Browser* browser)
    : SidePanelUIBase(browser) {
  // TODO(webium): Currently only reading list and bookmarks side panel
  // coordinators are constructed prior to this call. For the remaining
  // global entries, we should either move construction to
  // BrowserWindowFeatures::Init() or else explicitly disable support.
  SidePanelUtil::PopulateGlobalEntries(browser,
                                       SidePanelRegistry::From(browser));
}

WebUIBrowserSidePanelUI::~WebUIBrowserSidePanelUI() = default;

void WebUIBrowserSidePanelUI::Close(SidePanelEntry::PanelType panel_type,
                                    SidePanelEntryHideReason reason,
                                    bool supress_animations) {
  if (!IsSidePanelShowing(panel_type)) {
    return;
  }

  if (SidePanelEntry* entry = GetEntryForUniqueKey(*current_key(panel_type))) {
    entry->OnEntryWillHide(reason);
  }
  // Asynchronously close the side panel in webshell.
  // WebUI then notifies the browser when the side panel is actually closed
  // via OnSidePanelClosed().
  GetWebUIBrowserWindow()->CloseSidePanel();
}

void WebUIBrowserSidePanelUI::Toggle(SidePanelEntryKey key,
                                     SidePanelOpenTrigger open_trigger) {}

content::WebContents* WebUIBrowserSidePanelUI::GetWebContentsForTest(
    SidePanelEntryId id) {
  return nullptr;
}

void WebUIBrowserSidePanelUI::ShowFrom(
    SidePanelEntryKey entry_key,
    gfx::Rect starting_bounds_in_browser_coordinates) {
  // Show animation from starting_bounds_in_browser_coordinates is not supported
  // for webui side panel, instead trigger to show normally.
  SidePanelUI::Show(entry_key);
}

void WebUIBrowserSidePanelUI::DisableAnimationsForTesting() {}

void WebUIBrowserSidePanelUI::SetNoDelaysForTesting(
    bool no_delays_for_testing) {}

content::WebContents* WebUIBrowserSidePanelUI::GetWebContentsForId(
    SidePanelEntryId entry_id) const {
  // Current assumes one and only one side panel is showing.
  // TODO(webium): find web contents for entry_id.
  return GetContainingWebContents(current_side_panel_view_.get());
}

void WebUIBrowserSidePanelUI::Show(
    const UniqueKey& input,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // Side panel is not supported for non-normal browsers.
  if (!browser()->is_type_normal()) {
    return;
  }

  SidePanelEntry* entry = GetEntryForUniqueKey(input);
  if (IsSidePanelShowing(entry->type()) &&
      *current_key(entry->type()) == input) {
    waiter(entry->type())->ResetLoadingEntryIfNecessary();

    // TODO(webium): Implement the following:
    // If the side panel is in the process of closing, show it instead.
    // if (browser_view_->contents_height_side_panel()->state() ==
    // SidePanel::State::kClosing) {
    // browser_view_->contents_height_side_panel()->Open(/*animated=*/true);
    // NotifyPinnedContainerOfActiveStateChange(entry->key(), true);
    // }
    return;
  }

  waiter(entry->type())
      ->WaitForEntry(
          entry,
          base::BindOnce(&WebUIBrowserSidePanelUI::PopulateSidePanel,
                         base::Unretained(this), suppress_animations, input,
                         /*open_trigger=*/std::nullopt));
}

void WebUIBrowserSidePanelUI::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  if (IsSidePanelShowing(entry->type())) {
    SidePanelEntry* previous_entry =
        GetEntryForUniqueKey(*current_key(entry->type()));
    if (previous_entry) {
      previous_entry->OnEntryWillHide(SidePanelEntryHideReason::kReplaced);

      // TODO(webium): Call previous_entry->OnEntryHidden() below when the entry
      // is swapped.

      previous_entry->CacheView(std::move(current_side_panel_view_));
      current_side_panel_view_.reset();
    }
  }

  current_side_panel_view_ =
      content_view ? std::move(content_view.value()) : entry->GetContent();
  // This view is never attached to a widget, so override the focus manager
  // to use the top level widget's focus manager. This makes sure that
  // View::GetFocusManager() does not crash (called during key event dispatch).
  current_side_panel_view_->SetProperty(
      views::kDetachedViewFocusManagerKey,
      GetWebUIBrowserWindow()->widget()->GetFocusManager());
  SetCurrentKey(entry->type(), unique_key);
  GetWebUIBrowserWindow()->ShowSidePanel(entry->key());

  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntryFor(
        SidePanelEntry::PanelType::kContent);
  }

  entry->OnEntryShown();

  // TODO(webium): notify previous Entry::OnEntryHidden() if applicable.

  // TODO(webium): lens side panel will be closed on opening other side panels.
  // It observes the side panel opening using
  // SidePanelCoordinator::RegisterSidePanelShown().
}

void WebUIBrowserSidePanelUI::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  // Show an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  SidePanelEntry::PanelType panel_type = SidePanelEntry::PanelType::kContent;
  std::optional<UniqueKey> unique_key =
      IsSidePanelShowing(panel_type) ? GetNewActiveKeyOnTabChanged(panel_type)
                                     : std::nullopt;
  if (!unique_key.has_value() && new_contextual_registry &&
      new_contextual_registry->GetActiveEntryFor(panel_type).has_value()) {
    unique_key = UniqueKey{
        browser()->GetActiveTabInterface()->GetHandle(),
        (*new_contextual_registry->GetActiveEntryFor(panel_type))->key()};
  }

  if (unique_key.has_value()) {
    Show(*unique_key, SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
         /*suppress_animations=*/true);
    return;
  }

  // Store the old side panel, if there is one.
  if (old_contextual_registry &&
      old_contextual_registry->GetActiveEntryFor(panel_type).has_value() &&
      IsSidePanelShowing(panel_type) &&
      (*old_contextual_registry->GetActiveEntryFor(panel_type))->key() ==
          current_key(panel_type)->key &&
      current_key(panel_type)->tab_handle) {
    auto* active_entry =
        old_contextual_registry->GetActiveEntryFor(panel_type).value();
    active_entry->CacheView(std::move(std::move(current_side_panel_view_)));
    current_side_panel_view_.reset();
  }

  Close(panel_type, SidePanelEntryHideReason::kSidePanelClosed,
        /*suppress_animations=*/true);
}

void WebUIBrowserSidePanelUI::OnSidePanelClosed(
    SidePanelEntry::PanelType type) {
  if (!IsSidePanelShowing(type)) {
    return;
  }

  SidePanelEntry* previous_entry = GetEntryForUniqueKey(*current_key(type));
  SetCurrentKey(type, std::nullopt);
  if (previous_entry) {
    previous_entry->OnEntryHidden();
  }

  // Reset active entry values for all observed registries and clear cache for
  // everything except remaining active entries (i.e. if another tab has an
  // active contextual entry).
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntryFor(
        SidePanelEntry::PanelType::kContent);
  }

  SidePanelRegistry::From(browser())->ResetActiveEntryFor(
      SidePanelEntry::PanelType::kContent);

  current_side_panel_view_.reset();
  // TODO(webium): Clear cached views for registry entries for global and
  // contextual registries.
}

WebUIBrowserWindow* WebUIBrowserSidePanelUI::GetWebUIBrowserWindow() {
  return static_cast<WebUIBrowserWindow*>(browser()->window());
}
